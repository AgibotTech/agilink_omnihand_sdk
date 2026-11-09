// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include "omnihand/impl/eyou_motor_ota_upgrader.h"
#include "omnihand/impl/eyou_motor_ota_channel.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace agilink {
namespace omnihand {
namespace {

using Status = OtaProgressStatus;
using Error = OtaErrorCode;

struct ProgressEvent {
  int current;
  int total;
  Status status;
};

class EyouMotorOtaUpgraderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    static std::atomic<unsigned> counter{0};
    path_ = std::filesystem::temp_directory_path() /
        ("omnihand_motor_ota_" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
         "_" + std::to_string(counter++) + ".bin");
    WriteFirmware(2051);
  }
  void TearDown() override {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
    EXPECT_EQ(switches_.size() % 2, 0);
    for (size_t index = 0; index < switches_.size(); ++index) {
      EXPECT_EQ(switches_[index], index % 2 == 0);
    }
  }
  void WriteFirmware(size_t size) {
    firmware_.resize(size);
    for (size_t index = 0; index < size; ++index) {
      firmware_[index] = static_cast<uint8_t>(index * 37 + 11);
    }
    std::ofstream file(path_, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(firmware_.data()),
               static_cast<std::streamsize>(firmware_.size()));
    ASSERT_TRUE(file);
  }
  EyouMotorOtaUpgrader Engine() {
    return EyouMotorOtaUpgrader(
        [this](const EyouMotorOtaFrame& frame) {
          sent_.push_back(frame);
          if (throw_send_) throw std::runtime_error("CAN transport failed");
          if (frame.data[0] == 0x23) {
            ++enter_boot_count_;
            if (!stay_in_app_) mode_ = 0x55;
          } else if ((frame.data[1] & 0xF0) == 0x10) {
            block_.assign(frame.data.begin() + 5, frame.data.end());
            block_size_ = ((frame.data[1] & 0xF) << 8) + frame.data[2] - 2;
          } else if ((frame.data[1] & 0xF0) == 0x20) {
            block_.insert(block_.end(), frame.data.begin() + 2, frame.data.end());
          } else if (frame.data[3] == 0x06) {
            ++enter_app_count_;
            if (fail_jump_) return false;
            if (!stay_in_boot_) mode_ = 0xAA;
          }
          return !fail_send_;
        },
        [this](const EyouMotorOtaFrame& frame, uint16_t reply_id, int timeout) {
          requests_.push_back(frame);
          timeouts_.push_back(timeout);
          if (frame.data[2] == 0xB2) {
            ++boot_queries_;
            EXPECT_EQ(reply_id, 0xFF);
            if (canopen_app_ && mode_ == 0xAA) {
              return std::optional<EyouMotorOtaFrame>{};
            }
            return std::optional<EyouMotorOtaFrame>{{
                0xFF, {0xFE, 0x06, 0xF2, mode_, 1, 2, 3, 4}}};
          }
          if (frame.data[0] == 0x40) {
            EXPECT_EQ(frame.can_id, 0x600 + node_);
            EXPECT_EQ(reply_id, 0x580 + node_);
            return std::optional<EyouMotorOtaFrame>{{
                reply_id, {0x47, 0x0A, 0x10, 0, '1', '.', '2', 0}}};
          }
          EXPECT_EQ(frame.can_id, node_);
          EXPECT_EQ(reply_id, 0xFF);
          const uint8_t operation = frame.data[3];
          if (operation == timeout_operation_) {
            return std::optional<EyouMotorOtaFrame>{};
          }
          if (operation == 0x42) {
            erase_size_ = BigEndian32(frame.data.data() + 4);
          } else if (operation == 0x03) {
            offsets_.push_back(BigEndian32(frame.data.data() + 4));
          } else if (operation == 0x85 && failure_operation_ != 0x85) {
            // Independent receiver reconstructs exact bytes and verifies CRC.
            EXPECT_GE(block_.size(), block_size_);
            if (block_.size() < block_size_) return std::optional<EyouMotorOtaFrame>{};
            uint16_t crc = 0;
            for (size_t i = 0; i < block_size_; ++i) {
              crc ^= static_cast<uint16_t>(block_[i]) << 8;
              for (int bit = 0; bit < 8; ++bit) {
                crc = static_cast<uint16_t>((crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0));
              }
            }
            EXPECT_EQ(frame.data[4], crc >> 8);
            EXPECT_EQ(frame.data[5], crc & 0xFF);
            written_.insert(written_.end(), block_.begin(), block_.begin() + block_size_);
          }
          const uint8_t status = failure_operation_ == operation ? 7 : 0;
          // Reference idriver only checks FE 06 F4 STATUS; no required padding.
          return std::optional<EyouMotorOtaFrame>{{
              0xFF, {0xFE, 0x06, 0xF4, status,
                     static_cast<uint8_t>(buffer_size_ >> 8),
                     static_cast<uint8_t>(buffer_size_), 0x12, 0x34}}};
        },
        [this](bool enabled) {
          switches_.push_back(enabled);
          if ((enabled && throw_enter_) || (!enabled && throw_exit_)) {
            throw std::runtime_error("pass-through transport failed");
          }
          return enabled ? !fail_enter_ : !fail_exit_;
        },
        [this](std::chrono::microseconds duration) { sleeps_.push_back(duration.count()); });
  }
  OtaProgressCallback Progress() {
    return [this](int current, int total, Status status) {
      events_.push_back({current, total, status});
    };
  }
  static uint32_t BigEndian32(const uint8_t* data) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) | data[3];
  }
  void ExpectFailure() {
    ASSERT_FALSE(events_.empty());
    EXPECT_EQ(events_.back().status, Status::AGILINK_OTA_ERROR);
    EXPECT_TRUE(std::none_of(events_.begin(), events_.end(),
        [](const auto& event) { return event.status == Status::AGILINK_OTA_SUCCESS; }));
  }

  std::filesystem::path path_;
  std::vector<uint8_t> firmware_, block_, written_;
  std::vector<EyouMotorOtaFrame> sent_, requests_;
  std::vector<uint32_t> offsets_;
  std::vector<int> timeouts_;
  std::vector<int64_t> sleeps_;
  std::vector<ProgressEvent> events_;
  size_t block_size_ = 0;
  uint8_t node_ = 17;
  uint8_t mode_ = 0x55;
  uint8_t failure_operation_ = 0;
  uint8_t timeout_operation_ = 0;
  uint16_t buffer_size_ = 1026;
  uint32_t erase_size_ = 0;
  int boot_queries_ = 0;
  int enter_boot_count_ = 0;
  int enter_app_count_ = 0;
  bool canopen_app_ = false;
  bool stay_in_app_ = false;
  bool stay_in_boot_ = false;
  bool fail_jump_ = false;
  bool fail_send_ = false;
  bool throw_send_ = false;
  bool fail_enter_ = false;
  bool fail_exit_ = false;
  bool throw_enter_ = false;
  bool throw_exit_ = false;
  std::vector<bool> switches_;
};

TEST_F(EyouMotorOtaUpgraderTest, BootRecoveryStartsAtZeroAndVerifiesApp) {
  auto engine = Engine();
  ASSERT_TRUE(engine.UpdateFirmware(node_, path_.u8string(), Progress()));
  EXPECT_EQ(enter_boot_count_, 0);
  EXPECT_EQ(enter_app_count_, 1);
  EXPECT_EQ(erase_size_, 3072);
  EXPECT_EQ(offsets_, (std::vector<uint32_t>{0, 1024, 2048}));
  EXPECT_EQ(written_, firmware_);
  EXPECT_EQ(timeouts_[2], 5000);
  EXPECT_EQ(events_.back().status, Status::AGILINK_OTA_SUCCESS);
  EXPECT_EQ(events_.back().current, 3);
  EXPECT_EQ(events_.back().total, 3);
  EXPECT_EQ(std::count(sleeps_.begin(), sleeps_.end(), 2000), 342);
}

TEST_F(EyouMotorOtaUpgraderTest, AppEntersBootOnlyThroughCanopen) {
  mode_ = 0xAA;
  auto engine = Engine();
  ASSERT_TRUE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_EQ(enter_boot_count_, 1);
  ASSERT_FALSE(sent_.empty());
  EXPECT_EQ(sent_[0].can_id, 0x611);
  EXPECT_EQ(sent_[0].data, (std::array<uint8_t, 8>{0x23, 9, 0x21, 0, 0x78, 0x56, 0x34, 0x12}));
  EXPECT_EQ(sleeps_, (std::vector<int64_t>{500000, 500000}));
}

TEST_F(EyouMotorOtaUpgraderTest, CanopenFallbackDetectsAppBeforeAndAfterUpgrade) {
  mode_ = 0xAA;
  canopen_app_ = true;
  auto engine = Engine();
  EXPECT_TRUE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_EQ(enter_boot_count_, 1);
  EXPECT_EQ(written_, firmware_);
}

TEST_F(EyouMotorOtaUpgraderTest, AppMustActuallyEnterBootBeforeErasing) {
  mode_ = 0xAA;
  stay_in_app_ = true;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_EQ(erase_size_, 0);
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, UnknownModeNeverErases) {
  mode_ = 0x33;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_TRUE(sent_.empty());
  EXPECT_EQ(erase_size_, 0);
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, EraseFailureStopsBeforeAnyWrite) {
  failure_operation_ = 0x42;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_TRUE(offsets_.empty());
  EXPECT_TRUE(sent_.empty());
  EXPECT_NE(engine.GetLastError().find("status 7"), std::string::npos);
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, PrepareFailureStopsBeforeData) {
  failure_operation_ = 0x03;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_TRUE(sent_.empty());
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, InsufficientBufferStopsBeforeData) {
  buffer_size_ = 1025;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_TRUE(sent_.empty());
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, CrcFailureNeverAdvancesProgressOrJumpsApp) {
  failure_operation_ = 0x85;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_EQ(offsets_, (std::vector<uint32_t>{0}));
  EXPECT_EQ(enter_app_count_, 0);
  EXPECT_TRUE(std::none_of(events_.begin(), events_.end(),
      [](const auto& event) { return event.status == Status::AGILINK_OTA_TRANSMITTING; }));
  EXPECT_EQ(events_.back().current, static_cast<int>(Error::AGILINK_OTA_CRC_CHECK_FAILED));
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, EachOperationTimeoutStopsUpgrade) {
  for (const uint8_t operation : {0x42, 0x03, 0x85}) {
    timeout_operation_ = operation;
    events_.clear();
    auto engine = Engine();
    EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
    EXPECT_EQ(enter_app_count_, 0);
    ExpectFailure();
  }
}

TEST_F(EyouMotorOtaUpgraderTest, FailedAppJumpOrAppDetectionCannotReportSuccess) {
  stay_in_boot_ = true;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_EQ(events_.back().current, static_cast<int>(Error::AGILINK_OTA_RESTART_TIMEOUT));
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, FailedAppJumpSendCannotReportSuccess) {
  fail_jump_ = true;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_EQ(enter_app_count_, 1);
  EXPECT_EQ(events_.back().current,
            static_cast<int>(Error::AGILINK_OTA_DEVICE_DISCONNECTED));
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, SendFailureAndExceptionStopUpgrade) {
  for (const bool throws : {false, true}) {
    fail_send_ = !throws;
    throw_send_ = throws;
    auto engine = Engine();
    EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
    ExpectFailure();
  }
}



TEST_F(EyouMotorOtaUpgraderTest, CallbackExceptionPropagates) {
  auto engine = Engine();
  EXPECT_THROW(engine.UpdateFirmware(node_, path_.u8string(),
      [](int, int, Status status) {
        if (status == Status::AGILINK_OTA_UPGRADE_ACCEPTED) {
          throw std::runtime_error("callback failed");
        }
      }, 0), std::runtime_error);
}

TEST_F(EyouMotorOtaUpgraderTest, FailedEntryStillAttemptsExitWithoutMotorTraffic) {
  fail_enter_ = true;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
  EXPECT_TRUE(requests_.empty());
  EXPECT_TRUE(sent_.empty());
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, EntryExceptionStillAttemptsExit) {
  throw_enter_ = true;
  auto engine = Engine();
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
  EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
  EXPECT_TRUE(requests_.empty());
  ExpectFailure();
}

TEST_F(EyouMotorOtaUpgraderTest, FailedExitCannotReportUpgradeSuccess) {
  for (bool throws : {false, true}) {
    fail_exit_ = !throws;
    throw_exit_ = throws;
    events_.clear();
    auto engine = Engine();
    EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 0));
    EXPECT_EQ(events_.back().current,
              static_cast<int>(Error::AGILINK_OTA_DEVICE_DISCONNECTED));
    ExpectFailure();
  }
}

TEST_F(EyouMotorOtaUpgraderTest, SuccessIsReportedOnlyAfterExit) {
  auto engine = Engine();
  EXPECT_TRUE(engine.UpdateFirmware(node_, path_.u8string(),
      [this](int, int, Status status) {
        if (status == Status::AGILINK_OTA_SUCCESS) {
          EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
        }
      }, 0));
}

TEST_F(EyouMotorOtaUpgraderTest, NonstandardCallbackExceptionStillExits) {
  auto engine = Engine();
  EXPECT_THROW(engine.UpdateFirmware(node_, path_.u8string(),
      [](int, int, Status status) {
        if (status == Status::AGILINK_OTA_UPGRADE_ACCEPTED) {
          throw 42;
        }
      }, 0), int);
  EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
}

TEST_F(EyouMotorOtaUpgraderTest, RealChannelHandlesImmediateRepliesAcrossWholeUpgrade) {
  WriteFirmware(1025);
  EyouMotorOtaChannel* ptr = nullptr;
  bool app = false;
  int crc_count = 0;
  EyouMotorOtaChannel channel([&](const EyouMotorOtaFrame& frame) {
    if (frame.data[2] == 0xB2) {
      ptr->OnFrame({0xFF, {0xFE, 6, 0xF2,
                          static_cast<uint8_t>(app ? 0xAA : 0x55), 1, 2, 3, 4}});
    } else if (frame.data[2] == 0xB4 && frame.data[1] == 6) {
      if (frame.data[3] == 6) {
        app = true;
      } else {
        if (frame.data[3] == 0x85) ++crc_count;
        ptr->OnFrame({0xFF, {0xFE, 6, 0xF2, 0x55, 1, 2, 3, 4}});
        ptr->OnFrame({0xFF, {0xFE, 6, 0xF4, 0, 4, 2, 0xFF, 0xFF}});
      }
    }
    return true;
  });
  ptr = &channel;
  std::vector<bool> switches;
  EyouMotorOtaUpgrader engine(
      [&](const EyouMotorOtaFrame& frame) { return channel.SendFrame(frame); },
      [&](const EyouMotorOtaFrame& frame, uint16_t id, int timeout) {
        return channel.Exchange(frame, id, timeout);
      },
      [&](bool enabled) { switches.push_back(enabled); return true; },
      [](std::chrono::microseconds) {});
  EXPECT_TRUE(engine.UpdateFirmware(node_, path_.u8string(), nullptr, 0));
  EXPECT_EQ(crc_count, 2);
  EXPECT_EQ(switches, (std::vector<bool>{true, false}));
}

TEST_F(EyouMotorOtaUpgraderTest, InvalidInputAndBadFilesNeverSendFrames) {
  auto engine = Engine();
  for (const uint8_t node : {0, 22}) {
    EXPECT_FALSE(engine.UpdateFirmware(node, path_.u8string(), Progress(), 0));
  }
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), -1));
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress(), 1000001));
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string() + ".missing", Progress()));
  EXPECT_EQ(events_.back().current, static_cast<int>(Error::AGILINK_OTA_FILE_NOT_FOUND));
  WriteFirmware(0);
  EXPECT_FALSE(engine.UpdateFirmware(node_, path_.u8string(), Progress()));
  EXPECT_EQ(events_.back().current, static_cast<int>(Error::AGILINK_OTA_FILE_EMPTY));
  EXPECT_TRUE(requests_.empty());
  EXPECT_TRUE(switches_.empty());
}

class EyouMotorScanTest : public ::testing::Test {
 protected:
  EyouMotorOtaUpgrader MakeScanner() {
    return EyouMotorOtaUpgrader(
        {},
        [this](const EyouMotorOtaFrame& frame, uint16_t reply_id,
               int timeout_ms) -> std::optional<EyouMotorOtaFrame> {
          requests_.push_back(frame);
          EXPECT_EQ(timeout_ms, 100);
          if (throw_exchange_) {
            throw std::runtime_error("scan transport failed");
          }
          if (!respond_) {
            return std::nullopt;
          }
          if (frame.can_id == 1 || frame.can_id == 12 || frame.can_id == 21) {
            EXPECT_EQ(reply_id, 0xFF);
            const uint8_t type = frame.can_id == 1 ? 0x55 :
                                 frame.can_id == 21 ? 0xAA : 0x11;
            return EyouMotorOtaFrame{0xFF, {0xFE, 6, 0xF2, type, 1, 2, 3, 4}};
          }
          if (frame.can_id == 0x609) {
            EXPECT_EQ(reply_id, 0x589);
            return EyouMotorOtaFrame{0x589, {0x43, 0x0A, 0x10, 0, 'A', '1', '.', '0'}};
          }
          return std::nullopt;
        },
        [this](bool enabled) {
          switches_.push_back(enabled);
          if (throw_toggle_) {
            throw std::runtime_error("scan toggle failed");
          }
          return enabled ? !fail_enter_ : !fail_exit_;
        });
  }

  std::vector<EyouMotorOtaFrame> requests_;
  std::vector<bool> switches_;
  bool respond_ = true;
  bool throw_exchange_ = false;
  bool fail_enter_ = false;
  bool fail_exit_ = false;
  bool throw_toggle_ = false;
};

TEST_F(EyouMotorScanTest, ScansGlobalNodesBootFirstAndReportsOnlyKnownModes) {
  auto scanner = MakeScanner();
  std::vector<std::tuple<uint8_t, bool, std::string>> found;
  ASSERT_TRUE(scanner.ScanFirmware([&](uint8_t node, bool in_boot,
                                      const std::string& version) {
    found.emplace_back(node, in_boot, version);
  }));
  EXPECT_EQ(found, (decltype(found){{1, true, "1.2.3.4"},
                                  {9, false, "A1.0"},
                                  {21, false, "1.2.3.4"}}));
  size_t index = 0;
  for (uint8_t node = 1; node <= 21; ++node) {
    ASSERT_LT(index, requests_.size());
    EXPECT_EQ(requests_[index].can_id, node);
    EXPECT_EQ(requests_[index++].data,
              (std::array<uint8_t, 8>{0xFE, 6, 0xB2, 0x80, 0xFF, 0xFF, 0xFF, 0xFF}));
    if (node != 1 && node != 12 && node != 21) {
      ASSERT_LT(index, requests_.size());
      EXPECT_EQ(requests_[index].can_id, 0x600 + node);
      EXPECT_EQ(requests_[index++].data,
                (std::array<uint8_t, 8>{0x40, 0x0A, 0x10, 0, 0, 0, 0, 0}));
    }
  }
  EXPECT_EQ(index, requests_.size());
  EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
}

TEST_F(EyouMotorScanTest, NoRepliesCompletesWithEmptyResults) {
  respond_ = false;
  auto scanner = MakeScanner();
  int found = 0;
  EXPECT_TRUE(scanner.ScanFirmware([&](uint8_t, bool, const std::string&) {
    ++found;
  }));
  EXPECT_EQ(found, 0);
  EXPECT_EQ(requests_.size(), 42);
  EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
}

TEST_F(EyouMotorScanTest, MissingCallbackDoesNotSendFrames) {
  auto scanner = MakeScanner();
  EXPECT_FALSE(scanner.ScanFirmware(nullptr));
  EXPECT_TRUE(requests_.empty());
  EXPECT_TRUE(switches_.empty());
}

TEST_F(EyouMotorScanTest, MissingTransportDoesNotSendFrames) {
  EyouMotorOtaUpgrader scanner({}, {}, {});
  EXPECT_FALSE(scanner.ScanFirmware([](uint8_t, bool, const std::string&) {}));
}




TEST_F(EyouMotorScanTest, TransportExceptionStopsScan) {
  throw_exchange_ = true;
  auto scanner = MakeScanner();
  EXPECT_FALSE(scanner.ScanFirmware([](uint8_t, bool, const std::string&) {}));
  EXPECT_EQ(requests_.size(), 1);
  EXPECT_EQ(scanner.GetLastError(), "scan transport failed");
  EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
}

TEST_F(EyouMotorScanTest, CallbackExceptionsPropagate) {
  auto scanner = MakeScanner();
  EXPECT_THROW(scanner.ScanFirmware([](uint8_t, bool, const std::string&) {
    throw std::runtime_error("scan callback failed");
  }), std::runtime_error);
  EXPECT_EQ(requests_.size(), 1);
  requests_.clear();
  EXPECT_THROW(scanner.ScanFirmware([](uint8_t, bool, const std::string&) {
    throw 42;
  }), int);
  EXPECT_EQ(requests_.size(), 1);
  EXPECT_EQ(switches_, (std::vector<bool>{true, false, true, false}));
}

TEST_F(EyouMotorScanTest, FailedEntryExitsWithoutQueryingNodes) {
  fail_enter_ = true;
  auto scanner = MakeScanner();
  EXPECT_FALSE(scanner.ScanFirmware([](uint8_t, bool, const std::string&) {}));
  EXPECT_TRUE(requests_.empty());
  EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
}

TEST_F(EyouMotorScanTest, FailedExitCannotReportScanSuccess) {
  fail_exit_ = true;
  auto scanner = MakeScanner();
  EXPECT_FALSE(scanner.ScanFirmware([](uint8_t, bool, const std::string&) {}));
  EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
  EXPECT_NE(scanner.GetLastError().find("exit"), std::string::npos);
}

TEST_F(EyouMotorScanTest, ToggleExceptionStillAttemptsCleanup) {
  throw_toggle_ = true;
  auto scanner = MakeScanner();
  EXPECT_FALSE(scanner.ScanFirmware([](uint8_t, bool, const std::string&) {}));
  EXPECT_TRUE(requests_.empty());
  EXPECT_EQ(switches_, (std::vector<bool>{true, false}));
}

TEST_F(EyouMotorScanTest, RepeatedScansEachHaveTheirOwnPassThroughSession) {
  auto scanner = MakeScanner();
  for (int iteration = 0; iteration < 2; ++iteration) {
    EXPECT_TRUE(scanner.ScanFirmware([](uint8_t, bool, const std::string&) {}));
  }
  EXPECT_EQ(switches_, (std::vector<bool>{true, false, true, false}));
}

TEST(EyouMotorOtaStatusTest, ReferenceStatusDoesNotDependOnUnusedBytes) {
  EyouMotorOtaFrame reply{0xFF, {0xFE, 6, 0xF4, 0, 0x12, 0x34, 0x56, 0x78}};
  ASSERT_TRUE(EyouMotorOtaProtocol::ParseOperationStatus(reply));
  EXPECT_EQ(*EyouMotorOtaProtocol::ParseOperationStatus(reply), 0);
  for (unsigned status = 1; status <= 255; ++status) {
    reply.data[3] = static_cast<uint8_t>(status);
    EXPECT_EQ(*EyouMotorOtaProtocol::ParseOperationStatus(reply), status);
  }
  reply.data[2] = 0xF2;
  EXPECT_FALSE(EyouMotorOtaProtocol::ParseOperationStatus(reply));
  reply.data[2] = 0xF4;
  reply.can_id = 1;
  EXPECT_FALSE(EyouMotorOtaProtocol::ParseOperationStatus(reply));
  reply.can_id = 0xFF;
  reply.size = 7;
  EXPECT_FALSE(EyouMotorOtaProtocol::ParseOperationStatus(reply));
}

TEST(EyouMotorOtaChannelTest, OtherReplyTypesCannotSatisfyCurrentOperation) {
  EyouMotorOtaChannel* ptr = nullptr;
  EyouMotorOtaChannel channel([&](const EyouMotorOtaFrame&) {
    ptr->OnFrame({0xFF, {0xFE, 6, 0xF2, 0x55, 1, 2, 3, 4}});
    ptr->OnFrame({0xFF, {0xFE, 6, 0xF4, 7, 0, 0, 0, 0}});
    return true;
  });
  ptr = &channel;
  const auto reply = channel.Exchange(EyouMotorOtaProtocol(1).Erase(1024), 0xFF, 10);
  ASSERT_TRUE(reply);
  EXPECT_EQ(reply->data[2], 0xF4);
  EXPECT_EQ(reply->data[3], 7);
  // A response arriving while no request is pending must not be reused.
  channel.OnFrame({0xFF, {0xFE, 6, 0xF4, 0, 0, 0, 0, 0}});
}

}  // namespace
}  // namespace omnihand
}  // namespace agilink

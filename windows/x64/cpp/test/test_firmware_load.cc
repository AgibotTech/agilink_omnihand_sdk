// Copyright (c) 2025, Agibot Co., Ltd.
// AGILINK OmniHand SDK is licensed under Mulan PSL v2.

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "omnihand/impl/firmware_file.h"
#include "utils/crc.h"

namespace agilink {
namespace omnihand {
namespace {

using Error = OtaErrorCode;
using detail::FirmwareLoadResult;
using detail::LoadFirmwareOnly;

class FirmwareLoadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
        ("omnihand-firmware-load-" + std::to_string(stamp));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(directory_, ec);
  }

  std::string Write(const std::vector<uint8_t>& bytes,
                    const std::string& name = "firmware.bin") {
    const auto path = directory_ / std::filesystem::u8path(name);
    std::ofstream file(path, std::ios::binary);
    if (!bytes.empty()) {
      file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    EXPECT_TRUE(file.good());
    return path.u8string();
  }

  static std::vector<uint8_t> Image(size_t size) {
    std::vector<uint8_t> bytes(size);
    for (size_t i = 0; i < size; ++i) {
      bytes[i] = static_cast<uint8_t>(i * 7 + 3);
    }
    return bytes;
  }

  static std::vector<uint8_t> Flatten(const FirmwareLoadResult& image) {
    std::vector<uint8_t> bytes;
    for (const auto& packet : image.packets) {
      bytes.insert(bytes.end(), packet.begin(), packet.end());
    }
    return bytes;
  }

  std::filesystem::path directory_;
};

TEST_F(FirmwareLoadTest, ReadsExactBinaryBytesWithoutPadding) {
  const auto bytes = Image(2051);
  const auto file = detail::ReadFirmwareFile(Write(bytes));
  EXPECT_EQ(file.error_code, Error::AGILINK_SUCCESS);
  EXPECT_EQ(file.bytes, bytes);
}

TEST_F(FirmwareLoadTest, SupportsUtf8FilePaths) {
  const auto bytes = Image(51);
  const auto path = Write(bytes, u8"触觉固件 空格.bin");
  EXPECT_EQ(detail::ReadFirmwareFile(path).bytes, bytes);
  const auto image = LoadFirmwareOnly(path);
  ASSERT_EQ(image.error_code, Error::AGILINK_SUCCESS);
  EXPECT_TRUE(std::equal(bytes.begin(), bytes.end(), image.packets[0].begin()));
}

TEST_F(FirmwareLoadTest, RejectsMissingFilesAndDirectories) {
  const auto missing = (directory_ / "missing.bin").u8string();
  EXPECT_EQ(detail::ReadFirmwareFile(missing).error_code, Error::AGILINK_OTA_FILE_NOT_FOUND);
  EXPECT_EQ(LoadFirmwareOnly(missing).error_code, Error::AGILINK_OTA_FILE_NOT_FOUND);
  EXPECT_EQ(detail::ReadFirmwareFile(directory_.u8string()).error_code,
            Error::AGILINK_OTA_FILE_NOT_FOUND);
}

TEST_F(FirmwareLoadTest, RejectsEmptyImagesWithoutPackets) {
  const auto path = Write({});
  EXPECT_EQ(detail::ReadFirmwareFile(path).error_code, Error::AGILINK_OTA_FILE_EMPTY);
  const auto image = LoadFirmwareOnly(path);
  EXPECT_EQ(image.error_code, Error::AGILINK_OTA_FILE_EMPTY);
  EXPECT_TRUE(image.packets.empty());
  EXPECT_EQ(image.total_packets, 0);
}

TEST_F(FirmwareLoadTest, ChecksProtocolSizeLimitBeforeReturningData) {
  const auto path = Write(Image(129));
  const auto rejected = detail::ReadFirmwareFile(path, 128);
  EXPECT_EQ(rejected.error_code, Error::AGILINK_OTA_INVALID_FIRMWARE);
  EXPECT_TRUE(rejected.bytes.empty());
  EXPECT_EQ(detail::ReadFirmwareFile(path, 129).error_code, Error::AGILINK_SUCCESS);
}

TEST_F(FirmwareLoadTest, PreservesBytesAndPadsOnlyTheFinalPacket) {
  for (size_t size : {1, 2047, 2048, 2049, 4096, 4097}) {
    SCOPED_TRACE(size);
    const auto bytes = Image(size);
    const auto image = LoadFirmwareOnly(Write(bytes));
    ASSERT_EQ(image.error_code, Error::AGILINK_SUCCESS);
    EXPECT_EQ(image.file_size_bytes, size);
    EXPECT_EQ(image.total_packets, (size + 2047) / 2048);
    auto expected = bytes;
    expected.resize(((size + 2047) / 2048) * 2048, 0xFF);
    EXPECT_EQ(Flatten(image), expected);
  }
}

TEST_F(FirmwareLoadTest, LeavesHeadersUntouchedUntilCallerSelectsAppRange) {
  auto bytes = Image(128 + 2051);
  std::fill_n(bytes.begin(), 4, 0x55);
  std::fill_n(bytes.begin() + 124, 4, 0xAA);
  const auto file = detail::ReadFirmwareFile(Write(bytes));
  ASSERT_EQ(file.error_code, Error::AGILINK_SUCCESS);
  EXPECT_EQ(file.bytes, bytes);
  const auto full = LoadFirmwareOnly(Write(bytes));
  EXPECT_EQ(full.file_size_bytes, bytes.size());
  const auto app = detail::PacketizeFirmware(file.bytes.data() + 128, file.bytes.size() - 128);
  ASSERT_EQ(app.error_code, Error::AGILINK_SUCCESS);
  EXPECT_EQ(app.file_size_bytes, 2051);
  auto expected = std::vector<uint8_t>(bytes.begin() + 128, bytes.end());
  expected.resize(4096, 0xFF);
  EXPECT_EQ(Flatten(app), expected);
}

TEST_F(FirmwareLoadTest, PaddedGlobalCrcMatchesIndependentCalculation) {
  const auto image = LoadFirmwareOnly(Write(Image(2049)));
  ASSERT_EQ(image.error_code, Error::AGILINK_SUCCESS);
  const auto padded = Flatten(image);
  uint16_t expected = 0xFFFF;
  for (uint8_t byte : padded) {
    expected ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      expected = (expected >> 1) ^ ((expected & 1) ? 0xA001 : 0);
    }
  }
  EXPECT_EQ(Crc16Modbus(padded.data(), padded.size()), expected);
}

TEST_F(FirmwareLoadTest, RejectsInvalidPacketInputsBeforeReadingMemory) {
  EXPECT_EQ(detail::PacketizeFirmware(nullptr, 0).error_code, Error::AGILINK_OTA_FILE_EMPTY);
  EXPECT_EQ(detail::PacketizeFirmware(nullptr, 1).error_code, Error::AGILINK_OTA_INVALID_FIRMWARE);
  const uint8_t byte = 0;
  const size_t too_large = static_cast<size_t>((std::numeric_limits<int>::max)()) * 2048 + 1;
  EXPECT_EQ(detail::PacketizeFirmware(&byte, too_large).error_code, Error::AGILINK_OTA_INVALID_FIRMWARE);
}

}  // namespace
}  // namespace omnihand
}  // namespace agilink

// Copyright (c) 2025, Agibot Co., Ltd.
// OmniLogger is licensed under Mulan PSL v2.

/**
 * @file agilink_logger.h
 * @brief Process-wide singleton logger built on OmniLogger.
 *
 * Call AgilinkLogger::init() once at startup to choose the output sink.
 * All subsequent calls to AgilinkLogger::get() / info() / infof() etc. use
 * that instance.  If init() is never called the first get() auto-initialises
 * to stdout with timestamps.
 *
 * Two init() overloads:
 *
 *   // ① FILE* sink (timestamps on by default):
 *   AgilinkLogger::init(stdout, LogLevel::INFO);
 *
 *   // ② Custom write_fn (timestamps off by default):
 *   AgilinkLogger::init(
 *       [](LogLevel lv, const char* msg, std::size_t len) {
 *           RCLCPP_INFO(node->get_logger(), "%.*s", static_cast<int>(len), msg);
 *       });
 *
 * Logging:
 *   AgilinkLogger::get().infof("CAN_RX", "frame %08x len=%d", id, len);
 *   AgilinkLogger::get().info("plain message");
 */

#ifndef AGILINK_AGILINK_LOGGER_H_
#define AGILINK_AGILINK_LOGGER_H_

#include "omnilogger.h"
#include <memory>
#include <mutex>

#if defined(_WIN32) || defined(_WIN64)
  #ifdef AGILINK_BUILDING_DLL
    #define AGILINK_EXPORT __declspec(dllexport)
  #else
    #define AGILINK_EXPORT __declspec(dllimport)
  #endif
#else
  #define AGILINK_EXPORT __attribute__((visibility("default")))
#endif

namespace agilink {

class AGILINK_EXPORT AgilinkLogger {
 public:
  // ── Initialization ──────────────────────────────────────────────────────────

  // FILE* sink: no external formatting, timestamps on by default.
  static void init(FILE* fp = stdout,
                   LogLevel min_level = LogLevel::VERBOSE,
                   std::chrono::milliseconds idle_interval = std::chrono::milliseconds(50),
                   bool with_timestamp = true);

  // Custom sink: caller usually adds its own timestamps (e.g. RCLCPP, syslog), off by default.
  static void init(OmniLogger<>::WriteFn write_fn,
                   OmniLogger<>::FlushFn flush_fn = nullptr,
                   LogLevel min_level = LogLevel::VERBOSE,
                   std::chrono::milliseconds idle_interval = std::chrono::milliseconds(50),
                   bool with_timestamp = false);

  // Returns the singleton; auto-initialises to stdout if init() was not called.
  static OmniLogger<>& get() noexcept;

  // After stopping producers, drain, join the worker and release its queues.
  // Do not invoke from a sink or a Windows DLL detach handler.
  static void shutdown() noexcept;

  static uint64_t new_source_id() noexcept;

 private:
  static std::unique_ptr<OmniLogger<>>& inst_() noexcept;
  static std::mutex& mtx_() noexcept;
};

}  // namespace agilink

#endif  // AGILINK_AGILINK_LOGGER_H_

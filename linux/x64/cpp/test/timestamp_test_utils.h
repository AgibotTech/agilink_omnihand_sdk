// Copyright (c) 2026, Agibot Co., Ltd.
// OmniHand SDK is licensed under Mulan PSL v2.
#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <string>
#include <gtest/gtest.h>
#include "agilink_logger.h"

namespace timestamp_test {

// Keep the fractional part as an integer to preserve all six digits.
inline std::string FormatUnixTimestampUs(int64_t timestamp_us) {
  if (timestamp_us <= 0) return "unavailable";
  const std::time_t seconds = static_cast<std::time_t>(timestamp_us / 1000000);
  std::tm local{};
#if defined(_WIN32)
  if (localtime_s(&local, &seconds) != 0) return "invalid";
#else
  if (localtime_r(&seconds, &local) == nullptr) return "invalid";
#endif
  char date[32]{};
  if (std::strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &local) == 0)
    return "invalid";
  char fraction[8]{};
  std::snprintf(fraction, sizeof(fraction), ".%06lld",
                static_cast<long long>(timestamp_us % 1000000));
  return std::string(date) + fraction;
}

inline ::testing::AssertionResult ValidateAndPrintTimestamp(
    int64_t timestamp_us, const char* operation, const char* tag) {
  const auto date = FormatUnixTimestampUs(timestamp_us);
  agilink::AgilinkLogger::get().infof(tag, "[%s] received_local=%s",
                                   operation, date.c_str());
  if (timestamp_us <= 0 || date.size() != 26)
    return ::testing::AssertionFailure() << operation << " has no valid receive timestamp";
  const int64_t host_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  // A broad clock-domain/unit check, not a claim of microsecond accuracy.
  constexpr int64_t kClockToleranceUs = 5000000;
  if (timestamp_us < host_us - kClockToleranceUs || timestamp_us > host_us + kClockToleranceUs)
    return ::testing::AssertionFailure() << operation << " timestamp differs from host Unix time by more than 5 seconds";
  return ::testing::AssertionSuccess();
}

}  // namespace timestamp_test

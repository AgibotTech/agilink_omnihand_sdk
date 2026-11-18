// Copyright (c) 2025, Agibot Co., Ltd.
// OmniLogger is licensed under Mulan PSL v2.
// You can use this software according to the terms and conditions of the
// Mulan PSL v2. You may obtain a copy of Mulan PSL v2 at:
//     http://license.coscl.org.cn/MulanPSL2

/**
 * @file omnilogger.h
 * @brief Bounded per-thread SPSC async logger.
 *
 * Architecture
 * ─────────────
 *  Producers call log():
 *    1. Compile-time level check (if constexpr) — zero cost when filtered.
 *    2. Atomic runtime level check — one relaxed load, no lock.
 *    3. Copy into the calling thread's preallocated SPSC queue.
 *    4. Publish with release ordering; wake the worker only when it is asleep.
 *
 *  Single worker thread:
 *    1. Drain a bounded batch from each producer, preserving each queue's order.
 *    2. Format deferred payloads and timestamps, then invoke sinks.
 *    3. Flush output and acknowledge captured per-queue fences.
 *    4. Reclaim exited producers after their queues drain; sleep only when idle.
 *
 *  Memory: one fixed queue allocated on each producer's first use or preallocate().
 *  Queue overflow drops the new record; producers never overwrite unread slots.
 *
 * Template parameters
 * ────────────────────
 *  SlotSize       – max bytes per message (truncated if longer).
 *  RingCap        – slots per producer; MUST be a power of two.
 *  CompileMinLv   – messages below this value are eliminated at compile time.
 *                   Useful in release builds: set to LogLevel::INFO to make all
 *                   debug() calls disappear with zero overhead.
 *
 * Usage — two creation paths
 * ────────────────────────────
 *  ① make_file()  —  FILE* sink; timestamps prepended by default (with_timestamp=true).
 *
 *    // stdout, INFO and above:
 *    auto log = OmniLogger<>::make_file(stdout, LogLevel::INFO);
 *    log->info("hello");   // → [I] [2025-11-06 17:29:45.123456] hello
 *
 *    // File sink, timestamps off (caller formats its own prefix):
 *    FILE* fp = std::fopen("/var/log/app.log", "a");
 *    auto log = OmniLogger<>::make_file(fp, LogLevel::INFO,
 *                                       std::chrono::milliseconds(50), false);
 *    log->warn("my own prefix: joint limit exceeded");
 *    log->shutdown();
 *    std::fclose(fp);
 *
 *  ② Generic constructor  —  custom write_fn; timestamps off by default (with_timestamp=false),
 *     because most custom sinks add their own formatting.
 *
 *    // ROS 2 logger sink:
 *    OmniLogger<> ros_log(
 *        [node](LogLevel lv, const char* msg, std::size_t len) {
 *            RCLCPP_INFO(node->get_logger(), "%.*s", static_cast<int>(len), msg);
 *        });
 *
 *    // SDK callback style — backward-compatible, logs at INFO:
 *    sdk->ShowDataDetails(true, [&ros_log](const std::string& s) { ros_log(s); });
 *
 * Formatted logging (printf-style)
 * ──────────────────────────────────
 *  logf / infof / debugf / warnf / errorf / verbosef accept a TAG and a printf
 *  format string. TAG is written as a [TAG] prefix before the message; pass
 *  nullptr or "" to omit it. Formatting uses a fixed caller-thread buffer.
 *  GCC/Clang check format strings at compile time.
 *
 *    static constexpr const char* TAG = "CAN_RX";
 *
 *    log->infof(TAG, "frame %08x len=%d", id, len);
 *    // → [I] [2025-11-06 17:29:45.123456] [CAN_RX] frame deadbeef len=8
 *
 *    log->debugf("HandCtrl", "joint %d angle=%.2f", joint_id, angle);
 *    log->warnf(TAG,         "timeout after %d ms", elapsed_ms);
 *    log->errorf("Init",     "device not found: %s", dev_name);
 *    log->logf(LogLevel::INFO,  nullptr, "no tag: val=%d", val);
 *
 *  // Synchronous flush (blocks until all pending messages are written):
 *  log->flush();
 */

#ifndef AGILINK_OMNILOGGER_H_
#define AGILINK_OMNILOGGER_H_

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace agilink {

enum class LogLevel : uint8_t {
  VERBOSE = 0,    // High-frequency / per-frame tracing; filter out in normal operation
  DEBUG = 1,      // Development and diagnostic information
  INFO = 2,       // Normal operational events
  WARN = 3,       // Recoverable anomalies or boundary conditions
  ERROR = 4,      // Errors that need attention but may not terminate the program
  MAX_LEVEL = 5,  // Sentinel: count of real levels; use as min_level to silence all output
};

template <std::size_t SlotSize = 1024,
          std::size_t RingCap = 2048,
          uint8_t CompileMinLv = 0>  // 0 = LogLevel::VERBOSE; nothing filtered at compile time
class OmniLogger {
 private:
  struct Producer;
  struct Sink;
  using SinkList = std::vector<std::shared_ptr<const Sink>>;

 public:
  using WriteFn = std::function<void(LogLevel, const char*, std::size_t)>;
  using FlushFn = std::function<void()>;
  // Zero is the default category; applications define other category values.
  using Category = uint16_t;
  // Payload bytes are owned by the queue. Formatting runs on the output worker.
  // Return bytes written, excluding a terminator, at most capacity - 1.
  using FormatFn = std::size_t (*)(std::string_view payload, char* output, std::size_t capacity);

  class Fence {
   public:
    bool ready() const noexcept {
      for (const auto& position : positions_) {
        if (position.producer->consumed.load(std::memory_order_acquire) < position.target) return false;
      }
      return true;
    }

   private:
    friend class OmniLogger;
    struct Position {
      std::shared_ptr<Producer> producer;
      uint64_t target;
    };
    std::vector<Position> positions_;
  };

  struct Record {
    LogLevel level;
    uint64_t source_id;
    uint64_t producer_id;
    // Sequence within producer_id; no shared counter on the logging path.
    uint64_t sequence;
    std::chrono::system_clock::time_point timestamp;
    std::string_view module;
    std::string_view message;
    Category category;
  };

  // Views are valid only during this worker-thread call; true consumes the record.
  using RecordFn = std::function<bool(const Record&)>;
  using DrainFn = std::function<void()>;

  // Failed output attempts, not lost-record counts. Only the worker updates
  // these counters; logging threads do no extra work on successful submission.
  struct OutputErrors {
    uint64_t write_failures{0};
    uint64_t flush_failures{0};
    uint64_t sink_write_failures{0};
    uint64_t sink_drain_failures{0};
    // Most recent built-in FILE* error; zero until a file operation fails.
    int last_file_errno{0};
  };

  // Individual counters are atomic; concurrent output can advance between loads.
  OutputErrors output_errors() const noexcept {
    return {write_failures_.load(std::memory_order_relaxed),
            flush_failures_.load(std::memory_order_relaxed),
            sink_write_failures_.load(std::memory_order_relaxed),
            sink_drain_failures_.load(std::memory_order_relaxed),
            last_file_errno_.load(std::memory_order_relaxed)};
  }

  uint64_t add_sink(RecordFn write, DrainFn drained = {}) {
    const auto& state = sinks_;
    std::lock_guard<std::mutex> lock(state->mutex);
    auto next = std::make_shared<SinkList>(*std::atomic_load(&state->list));
    const auto id = ++state->next_id;
    next->push_back(std::make_shared<Sink>(Sink{id, std::move(write), std::move(drained)}));
    std::atomic_store(&state->list, std::shared_ptr<const SinkList>(std::move(next)));
    return id;
  }

  void remove_sink(uint64_t id) {
    const auto& state = sinks_;
    std::shared_ptr<const SinkList> previous;
    {
      std::lock_guard<std::mutex> lock(state->mutex);
      previous = std::atomic_load(&state->list);
      auto next = std::make_shared<SinkList>();
      for (const auto& sink : *previous) {
        if (sink->id != id) next->push_back(sink);
      }
      std::atomic_store(&state->list, std::shared_ptr<const SinkList>(std::move(next)));
    }
  }

  Fence capture_fence() const {
    Fence fence;
    std::lock_guard<std::mutex> lock(producers_mutex_);
    fence.positions_.reserve(producers_.size());
    for (const auto& producer : producers_) {
      fence.positions_.push_back({producer, producer->published.load(std::memory_order_acquire)});
    }
    return fence;
  }

  uint64_t dropped_count() const noexcept {
    std::lock_guard<std::mutex> lock(producers_mutex_);
    uint64_t count = retired_dropped_ + allocation_dropped_.load(std::memory_order_relaxed);
    for (const auto& producer : producers_) count += producer->dropped.load(std::memory_order_relaxed);
    return count;
  }

  std::size_t producer_count() const {
    std::lock_guard<std::mutex> lock(producers_mutex_);
    return producers_.size();
  }

  // Call on each latency-sensitive thread before entering its control loop.
  // First use registers and prefaults this thread's queue; subsequent use is cached.
  bool preallocate() noexcept {
    return !stopping_.load(std::memory_order_relaxed) && get_producer() != nullptr;
  }

  static_assert(RingCap > 0 && (RingCap & (RingCap - 1)) == 0, "RingCap must be a power of two");
  static_assert(SlotSize >= 8 && SlotSize <= 65535, "SlotSize must fit a 16-bit length");
  static constexpr std::size_t kMask = RingCap - 1;
  static constexpr std::size_t kMaxMessageSize = SlotSize - 1;

  /**
   * @param write_fn        Called by the worker for each message. Exceptions are counted.
   * @param flush_fn        Flush pending output every 100 ms, on ERROR, flush() or shutdown.
   *                       May be nullptr. Record and drain callbacks remain immediate.
   * @param min_level       Runtime minimum level; overrides CompileMinLv if higher.
   * @param idle_interval   Idle wake interval; queued records wake the worker immediately.
   * @param with_timestamp  Prepend [YYYY-mm-dd HH:MM:SS.uuuuuu] to every message (local time).
   *                        Defaults to false here — custom write_fn usually handles its own
   *                        formatting (e.g. RCLCPP already adds timestamps).
   *                        make_file() defaults to true.
   */
  explicit OmniLogger(WriteFn write_fn,
                      FlushFn flush_fn = nullptr,
                      LogLevel min_level = LogLevel::DEBUG,
                      std::chrono::milliseconds idle_interval = std::chrono::milliseconds(50),
                      bool with_timestamp = false)
      : signal_(std::make_shared<WakeSignal>()),
        write_fn_(std::move(write_fn)),
        flush_fn_(std::move(flush_fn)),
        min_level_(static_cast<uint8_t>(min_level)),
        idle_interval_(idle_interval),
        with_timestamp_(with_timestamp),
        worker_(&OmniLogger::run, this) {}

  ~OmniLogger() {
    shutdown();
  }

  OmniLogger(const OmniLogger&) = delete;
  OmniLogger& operator=(const OmniLogger&) = delete;
  OmniLogger(OmniLogger&&) = delete;
  OmniLogger& operator=(OmniLogger&&) = delete;

  // Stop producers before reconfiguring. Pending output drains to the old writer;
  // subscriptions are retained. Do not call from a sink.
  void configure(WriteFn write_fn, FlushFn flush_fn = nullptr,
                 LogLevel min_level = LogLevel::DEBUG,
                 std::chrono::milliseconds idle_interval = std::chrono::milliseconds(50),
                 bool with_timestamp = false) {
    auto identity = std::make_shared<const int>(0);
    shutdown();
    identity_ = std::move(identity);
    write_fn_ = std::move(write_fn);
    flush_fn_ = std::move(flush_fn);
    min_level_.store(static_cast<uint8_t>(min_level), std::memory_order_relaxed);
    idle_interval_ = idle_interval;
    with_timestamp_ = with_timestamp;
    retired_dropped_ = 0;
    allocation_dropped_.store(0, std::memory_order_relaxed);
    write_failures_.store(0, std::memory_order_relaxed);
    flush_failures_.store(0, std::memory_order_relaxed);
    sink_write_failures_.store(0, std::memory_order_relaxed);
    sink_drain_failures_.store(0, std::memory_order_relaxed);
    last_file_errno_.store(0, std::memory_order_relaxed);
    stopping_.store(false, std::memory_order_relaxed);
    try {
      worker_ = std::thread(&OmniLogger::run, this);
    } catch (...) {
      stopping_.store(true, std::memory_order_relaxed);
      throw;
    }
  }

  void configure(FILE* fp, LogLevel min_level = LogLevel::DEBUG,
                 std::chrono::milliseconds idle_interval = std::chrono::milliseconds(50),
                 bool with_timestamp = true) {
    configure(file_writer(fp), file_flusher(fp), min_level, idle_interval, with_timestamp);
  }

  // ── Logging ────────────────────────────────────────────────────────────────

  void log(LogLevel level, std::string_view msg) noexcept {
    log_impl(level, msg, nullptr);
  }

  // Use the captured event time when formatting timestamps; output stays async.
  void log(LogLevel level, std::string_view msg,
           std::chrono::system_clock::time_point event_time) noexcept {
    log_impl(level, msg, &event_time);
  }

  // Severity, source and category are independent structured attributes.
  void log(LogLevel level, uint64_t source_id, std::string_view msg,
           Category category = 0) noexcept {
    log_impl(level, msg, nullptr, source_id, 0, category);
  }

  void log(LogLevel level, uint64_t source_id, std::string_view msg,
           std::chrono::system_clock::time_point event_time,
           Category category = 0) noexcept {
    log_impl(level, msg, &event_time, source_id, 0, category);
  }

  // Call before preparing expensive arguments; log() also checks on submission.
  bool should_log(LogLevel level) const noexcept {
    if constexpr (CompileMinLv > 0) {
      if (static_cast<uint8_t>(level) < CompileMinLv) return false;
    }
    return static_cast<uint8_t>(level) >= min_level_.load(std::memory_order_relaxed) &&
           !stopping_.load(std::memory_order_relaxed);
  }

  // Dynamic-level entry point; use verbose/debug/info/warn/error for fixed levels.
  bool log_deferred(LogLevel level, uint64_t source_id, std::string_view payload,
                    FormatFn format, std::chrono::system_clock::time_point event_time,
                    Category category = 0) noexcept {
    if (!should_log(level) || !format) return false;
    return enqueue(level, payload, event_time, source_id, 0, category, format);
  }

  bool log_deferred(LogLevel level, uint64_t source_id, std::string_view payload,
                    FormatFn format, Category category = 0) noexcept {
    if (!should_log(level) || !format) return false;
    return enqueue(level, payload, std::chrono::system_clock::now(), source_id, 0, category, format);
  }

  void verbose(uint64_t source_id, std::string_view message, Category category = 0) noexcept {
    log(LogLevel::VERBOSE, source_id, message, category);
  }

  void verbose(uint64_t source_id, std::string_view message,
               std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    log(LogLevel::VERBOSE, source_id, message, event_time, category);
  }

  void debug(uint64_t source_id, std::string_view message, Category category = 0) noexcept {
    log(LogLevel::DEBUG, source_id, message, category);
  }

  void debug(uint64_t source_id, std::string_view message,
             std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    log(LogLevel::DEBUG, source_id, message, event_time, category);
  }

  void info(uint64_t source_id, std::string_view message, Category category = 0) noexcept {
    log(LogLevel::INFO, source_id, message, category);
  }

  void info(uint64_t source_id, std::string_view message,
            std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    log(LogLevel::INFO, source_id, message, event_time, category);
  }

  void warn(uint64_t source_id, std::string_view message, Category category = 0) noexcept {
    log(LogLevel::WARN, source_id, message, category);
  }

  void warn(uint64_t source_id, std::string_view message,
            std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    log(LogLevel::WARN, source_id, message, event_time, category);
  }

  void error(uint64_t source_id, std::string_view message, Category category = 0) noexcept {
    log(LogLevel::ERROR, source_id, message, category);
  }

  void error(uint64_t source_id, std::string_view message,
             std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    log(LogLevel::ERROR, source_id, message, event_time, category);
  }

  bool verbose(uint64_t source_id, std::string_view payload, FormatFn format,
               std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    return log_deferred(LogLevel::VERBOSE, source_id, payload, format, event_time, category);
  }

  bool verbose(uint64_t source_id, std::string_view payload, FormatFn format,
               Category category = 0) noexcept {
    return log_deferred(LogLevel::VERBOSE, source_id, payload, format, category);
  }

  bool debug(uint64_t source_id, std::string_view payload, FormatFn format,
             std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    return log_deferred(LogLevel::DEBUG, source_id, payload, format, event_time, category);
  }

  bool debug(uint64_t source_id, std::string_view payload, FormatFn format,
             Category category = 0) noexcept {
    return log_deferred(LogLevel::DEBUG, source_id, payload, format, category);
  }

  bool info(uint64_t source_id, std::string_view payload, FormatFn format,
            std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    return log_deferred(LogLevel::INFO, source_id, payload, format, event_time, category);
  }

  bool info(uint64_t source_id, std::string_view payload, FormatFn format,
            Category category = 0) noexcept {
    return log_deferred(LogLevel::INFO, source_id, payload, format, category);
  }

  bool warn(uint64_t source_id, std::string_view payload, FormatFn format,
            std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    return log_deferred(LogLevel::WARN, source_id, payload, format, event_time, category);
  }

  bool warn(uint64_t source_id, std::string_view payload, FormatFn format,
            Category category = 0) noexcept {
    return log_deferred(LogLevel::WARN, source_id, payload, format, category);
  }

  bool error(uint64_t source_id, std::string_view payload, FormatFn format,
             std::chrono::system_clock::time_point event_time, Category category = 0) noexcept {
    return log_deferred(LogLevel::ERROR, source_id, payload, format, event_time, category);
  }

  bool error(uint64_t source_id, std::string_view payload, FormatFn format,
             Category category = 0) noexcept {
    return log_deferred(LogLevel::ERROR, source_id, payload, format, category);
  }

 private:
  void log_impl(LogLevel level, std::string_view msg,
                const std::chrono::system_clock::time_point* event_time,
                uint64_t source_id = 0, uint16_t module_len = 0, Category category = 0) noexcept {
    if (!should_log(level)) return;
    const auto timestamp = event_time ? *event_time : std::chrono::system_clock::now();
    (void)enqueue(level, msg.substr(0, kMaxMessageSize), timestamp, source_id, module_len, category, nullptr);
  }

 public:
  void verbose(std::string_view msg) noexcept {
    log(LogLevel::VERBOSE, msg);
  }

  void verbose(std::string_view msg, std::chrono::system_clock::time_point event_time) noexcept {
    log(LogLevel::VERBOSE, msg, event_time);
  }

  void debug(std::string_view msg) noexcept {
    log(LogLevel::DEBUG, msg);
  }

  void debug(std::string_view msg, std::chrono::system_clock::time_point event_time) noexcept {
    log(LogLevel::DEBUG, msg, event_time);
  }

  void info(std::string_view msg) noexcept {
    log(LogLevel::INFO, msg);
  }

  void info(std::string_view msg, std::chrono::system_clock::time_point event_time) noexcept {
    log(LogLevel::INFO, msg, event_time);
  }

  void warn(std::string_view msg) noexcept {
    log(LogLevel::WARN, msg);
  }

  void warn(std::string_view msg, std::chrono::system_clock::time_point event_time) noexcept {
    log(LogLevel::WARN, msg, event_time);
  }

  void error(std::string_view msg) noexcept {
    log(LogLevel::ERROR, msg);
  }

  void error(std::string_view msg, std::chrono::system_clock::time_point event_time) noexcept {
    log(LogLevel::ERROR, msg, event_time);
  }

  // ── Formatted logging (printf-style) ─────────────────────────────────────────

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 4, 5)))
#endif
  void
  logf(LogLevel level, const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(level, tag, fmt, ap);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 3, 4)))
#endif
  void verbosef(const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::VERBOSE, tag, fmt, ap);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 3, 4)))
#endif
  void debugf(const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::DEBUG, tag, fmt, ap);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 3, 4)))
#endif
  void infof(const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::INFO, tag, fmt, ap);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 3, 4)))
#endif
  void warnf(const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::WARN, tag, fmt, ap);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 3, 4)))
#endif
  void errorf(const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::ERROR, tag, fmt, ap);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 5, 6)))
#endif
  void
  logf(LogLevel level, uint64_t source_id, const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(level, tag, fmt, ap, source_id);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 4, 5)))
#endif
  void
  verbosef(uint64_t source_id, const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::VERBOSE, tag, fmt, ap, source_id);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 4, 5)))
#endif
  void
  debugf(uint64_t source_id, const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::DEBUG, tag, fmt, ap, source_id);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 4, 5)))
#endif
  void
  infof(uint64_t source_id, const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::INFO, tag, fmt, ap, source_id);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 4, 5)))
#endif
  void
  warnf(uint64_t source_id, const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::WARN, tag, fmt, ap, source_id);
    va_end(ap);
  }

#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 4, 5)))
#endif
  void
  errorf(uint64_t source_id, const char* tag, const char* fmt, ...) noexcept {
    va_list ap;
    va_start(ap, fmt);
    vlogf_impl(LogLevel::ERROR, tag, fmt, ap, source_id);
    va_end(ap);
  }

  // ── Multi-line helpers ────────────────────────────────────────────────────────
  // Splits `s` on '\n' and logs each non-empty line individually.
  // Useful for ToString() output that spans multiple lines.

  void log_lines(LogLevel level, const char* tag, const char* s) noexcept {
    const char* p = s;
    while (*p) {
      const char* nl  = std::strchr(p, '\n');
      const char* end = nl ? nl : p + std::strlen(p);
      if (end > p) logf(level, tag, "%.*s", static_cast<int>(end - p), p);
      p = nl ? nl + 1 : end;
    }
  }

  void info_lines(const char* tag, const char* s) noexcept { log_lines(LogLevel::INFO, tag, s); }
  void warn_lines(const char* tag, const char* s) noexcept { log_lines(LogLevel::WARN, tag, s); }
  void error_lines(const char* tag, const char* s) noexcept { log_lines(LogLevel::ERROR, tag, s); }
  void debug_lines(const char* tag, const char* s) noexcept { log_lines(LogLevel::DEBUG, tag, s); }

  void log_lines(LogLevel level, uint64_t source_id, const char* tag, const char* text) noexcept {
    const char* p = text;
    while (*p) {
      const char* nl = std::strchr(p, '\n');
      const char* end = nl ? nl : p + std::strlen(p);
      if (end > p) logf(level, source_id, tag, "%.*s", static_cast<int>(end - p), p);
      p = nl ? nl + 1 : end;
    }
  }
  void info_lines(uint64_t source, const char* tag, const char* s) noexcept { log_lines(LogLevel::INFO, source, tag, s); }
  void warn_lines(uint64_t source, const char* tag, const char* s) noexcept { log_lines(LogLevel::WARN, source, tag, s); }
  void error_lines(uint64_t source, const char* tag, const char* s) noexcept { log_lines(LogLevel::ERROR, source, tag, s); }
  void debug_lines(uint64_t source, const char* tag, const char* s) noexcept { log_lines(LogLevel::DEBUG, source, tag, s); }

  // Backward-compatible callback style: operator()(line) logs at INFO.
  void operator()(const std::string& line) noexcept {
    log(LogLevel::INFO, line);
  }

  void set_min_level(LogLevel level) noexcept {
    min_level_.store(static_cast<uint8_t>(level), std::memory_order_relaxed);
  }

  /**
   * @brief Block until all messages enqueued before this call have been
   *        delivered and output flushed, or attempted if an output fails.
   *        Inspect output_errors() for failures. Later records do not extend the fence.
   *        Calling from a sink returns immediately to avoid waiting on itself.
   */
  void flush() noexcept {
    if (!worker_.joinable() || std::this_thread::get_id() == worker_.get_id()) return;
    FlushRequest request{capture_fence()};
    {
      std::lock_guard<std::mutex> lock(control_mutex_);
      flush_requests_.push_back(&request);
      controls_pending_.store(true, std::memory_order_release);
    }
    signal_->notify();
    std::unique_lock<std::mutex> lock(request.mutex);
    request.condition.wait(lock, [&request] { return request.complete; });
  }

  // Stop producers first. Drain, join and release queues during normal teardown;
  // repeated calls are safe. Do not call from a sink or a DLL detach handler.
  void shutdown() noexcept {
    stopping_.store(true, std::memory_order_release);
    signal_->notify();
    if (worker_.joinable()) worker_.join();
    worker_producers_.clear();
    std::lock_guard<std::mutex> lock(producers_mutex_);
    for (const auto& producer : producers_) retired_dropped_ += producer->dropped.load(std::memory_order_relaxed);
    producers_.clear();
  }

  // ── Factory ────────────────────────────────────────────────────────────────

  /**
   * @brief FILE* sink with level prefix. Flush every 100 ms while output is pending;
   *        ERROR, flush() and shutdown flush earlier.
   *
   * @param fp              Output file (default stdout).
   * @param min_level       Minimum level to write.
   * @param idle_interval   Idle wake interval; queued records wake the worker immediately.
   * @param with_timestamp  Prepend [YYYY-mm-dd HH:MM:SS.uuuuuu] to every line (default true).
   *                        Pass false when the caller formats its own prefix.
   *
   * LogLevel prefix: "[D]" / "[I]" / "[W]" / "[E]".
   */
  static std::unique_ptr<OmniLogger> make_file(
      FILE* fp = stdout,
      LogLevel min_level = LogLevel::DEBUG,
      std::chrono::milliseconds idle_interval = std::chrono::milliseconds(50),
      bool with_timestamp = true) {
    return std::make_unique<OmniLogger>(file_writer(fp), file_flusher(fp), min_level,
                                        idle_interval, with_timestamp);
  }

 private:
  struct FileError {
    int number;
  };

  static WriteFn file_writer(FILE* fp) {
    return [fp](LogLevel, const char* message, std::size_t size) {
      errno = 0;
      if (std::fwrite(message, 1, size, fp) != size) throw FileError{errno ? errno : EIO};
      if (size == 0 || message[size - 1] != '\n') {
        errno = 0;
        if (std::fputc('\n', fp) == EOF) throw FileError{errno ? errno : EIO};
      }
    };
  }

  static FlushFn file_flusher(FILE* fp) {
    return [fp]() {
      errno = 0;
      if (std::fflush(fp) == EOF) throw FileError{errno ? errno : EIO};
    };
  }

  // ── Internal types ─────────────────────────────────────────────────────────

  struct Slot {
    uint16_t len{0};
    LogLevel level{LogLevel::DEBUG};
    uint64_t source_id{0};
    std::chrono::system_clock::time_point timestamp{};
    uint16_t module_len{0};
    Category category{0};
    FormatFn format{nullptr};
    char     buf[SlotSize]{};
  };

  struct WakeSignal {
    std::mutex mutex;
    std::condition_variable condition;
    std::atomic<uint32_t> sleeping{0};

    void notify() noexcept {
      // Paired with the worker's fence after arming sleep. Without this
      // StoreLoad barrier, publication and the sleep check could both be missed.
      // Active producers do not write a shared event counter or acquire a mutex.
      std::atomic_thread_fence(std::memory_order_seq_cst);
      if (!sleeping.load(std::memory_order_acquire)) return;
      std::lock_guard<std::mutex> lock(mutex);
      sleeping.store(0, std::memory_order_release);
      condition.notify_one();
    }
  };

  struct Producer {
    explicit Producer(uint64_t producer_id, std::shared_ptr<WakeSignal> wake)
        : id(producer_id), thread_id(std::this_thread::get_id()), signal(std::move(wake)) {}

    // Only this producer modifies these counters and publishes new slots.
    alignas(64) uint64_t write_position = 0;
    uint64_t cached_read = 0;
    std::atomic<uint64_t> published{0};
    std::atomic<uint64_t> dropped{0};
    // Only the output worker modifies consumed, after callbacks return.
    alignas(64) std::atomic<uint64_t> consumed{0};
    alignas(64) std::atomic<bool> closed{false};
    // Header instantiations in different DLLs can cache this same thread queue.
    std::atomic<uint32_t> cache_users{1};
    const uint64_t id;
    const std::thread::id thread_id;
    const std::shared_ptr<WakeSignal> signal;
    alignas(64) std::array<Slot, RingCap> slots{};

    Slot* reserve() noexcept {
      if (write_position - cached_read == RingCap) {
        cached_read = consumed.load(std::memory_order_acquire);
        if (write_position - cached_read == RingCap) {
          dropped.fetch_add(1, std::memory_order_relaxed);
          return nullptr;
        }
      }
      return &slots[write_position & kMask];
    }

    void commit() noexcept {
      published.store(++write_position, std::memory_order_release);
      signal->notify();
    }
  };

  struct ThreadCache {
    bool* destroyed;
    struct Entry {
      // Retain only the small identity token, not queues of destroyed loggers.
      // Its address cannot be reused while this cache still refers to it.
      std::shared_ptr<const int> identity;
      std::weak_ptr<Producer> producer;
      Producer* pointer;
    };
    std::vector<Entry> entries;
    const int* last_identity = nullptr;
    Producer* last_producer = nullptr;

    ~ThreadCache() {
      *destroyed = true;
      for (const auto& entry : entries) {
        if (const auto producer = entry.producer.lock()) {
          if (producer->cache_users.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            producer->closed.store(true, std::memory_order_release);
            producer->signal->notify();
          }
        }
      }
    }
  };

  struct FlushRequest {
    Fence fence;
    std::mutex mutex;
    std::condition_variable condition;
    bool complete = false;
  };

  Producer* get_producer() noexcept {
    thread_local bool cache_destroyed = false;
    if (cache_destroyed) {
      allocation_dropped_.fetch_add(1, std::memory_order_relaxed);
      return nullptr;
    }
    thread_local ThreadCache cache{&cache_destroyed};
    if (cache.last_identity == identity_.get()) return cache.last_producer;
    for (auto it = cache.entries.begin(); it != cache.entries.end();) {
      if (it->identity.get() == identity_.get()) {
        cache.last_identity = identity_.get();
        cache.last_producer = it->pointer;
        return it->pointer;
      }
      if (it->producer.expired()) {
        it = cache.entries.erase(it);
      } else {
        ++it;
      }
    }
    try {
      // Registration and allocation occur once per producer, before steady use.
      std::lock_guard<std::mutex> lock(producers_mutex_);
      cache.entries.reserve(cache.entries.size() + 1);
      for (const auto& producer : producers_) {
        if (producer->thread_id == std::this_thread::get_id() &&
            !producer->closed.load(std::memory_order_acquire)) {
          cache.entries.push_back({identity_, producer, producer.get()});
          producer->cache_users.fetch_add(1, std::memory_order_relaxed);
          cache.last_identity = identity_.get();
          cache.last_producer = producer.get();
          return producer.get();
        }
      }
      producers_.reserve(producers_.size() + 1);
      auto producer = std::make_shared<Producer>(++next_producer_id_, signal_);
      cache.entries.push_back({identity_, producer, producer.get()});
      producers_.push_back(producer);
      producers_revision_.fetch_add(1, std::memory_order_release);
      cache.last_identity = identity_.get();
      cache.last_producer = producer.get();
      return producer.get();
    } catch (...) {
      allocation_dropped_.fetch_add(1, std::memory_order_relaxed);
      return nullptr;
    }
  }

  bool enqueue(LogLevel level, std::string_view payload,
               std::chrono::system_clock::time_point timestamp,
               uint64_t source_id, uint16_t module_len, Category category, FormatFn format) noexcept {
    Producer* producer = get_producer();
    if (!producer) return false;
    if (payload.size() > kMaxMessageSize) {
      producer->dropped.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    Slot* slot = producer->reserve();
    if (!slot) return false;
    slot->level = level;
    slot->source_id = source_id;
    slot->timestamp = timestamp;
    slot->module_len = module_len;
    slot->category = category;
    slot->format = format;
    slot->len = static_cast<uint16_t>(payload.size());
    if (!payload.empty()) std::memcpy(slot->buf, payload.data(), payload.size());
    producer->commit();
    return true;
  }

  // ── Worker ─────────────────────────────────────────────────────────────────

  void refresh_producers() {
    if (worker_revision_ == producers_revision_.load(std::memory_order_acquire)) return;
    std::lock_guard<std::mutex> lock(producers_mutex_);
    worker_producers_ = producers_;
    worker_revision_ = producers_revision_.load(std::memory_order_relaxed);
  }

  void dispatch(const Slot& slot, const Producer& producer, uint64_t sequence,
                const SinkList& sinks) noexcept {
    char formatted[SlotSize];
    std::string_view message(slot.buf, slot.len);
    if (slot.format) {
      try {
        const auto length = slot.format(message, formatted, sizeof(formatted));
        message = std::string_view(formatted, std::min(length, kMaxMessageSize));
      } catch (...) {
        message = "<log formatting failed>";
      }
    }
    const Record record{slot.level, slot.source_id, producer.id, sequence, slot.timestamp,
                        slot.module_len ? std::string_view(slot.buf + 1, slot.module_len) : std::string_view{},
                        message, slot.category};
    bool consumed = false;
    for (const auto& sink : sinks) {
      try {
        if (sink->write) consumed = sink->write(record) || consumed;
      } catch (...) {
        sink_write_failures_.fetch_add(1, std::memory_order_relaxed);
      }
    }
    if (!consumed) write_default(record);
  }

  bool drain_producer(Producer& producer, const SinkList& sinks) noexcept {
    // Bound each visit so one busy thread cannot starve other queues or controls.
    constexpr uint64_t kDrainBatch = 64;
    const auto read = producer.consumed.load(std::memory_order_relaxed);
    const auto available = producer.published.load(std::memory_order_acquire);
    const auto end = read + std::min(available - read, kDrainBatch);
    for (auto position = read; position < end; ++position) {
      dispatch(producer.slots[position & kMask], producer, position + 1, sinks);
    }
    producer.consumed.store(end, std::memory_order_release);
    return end != read;
  }

  void flush_output() noexcept {
    // A failed attempt must not cause an idle retry loop. New output or an
    // explicit flush can try again; records are never replayed.
    output_dirty_ = false;
    urgent_flush_ = false;
    if (!flush_fn_) return;
    try {
      flush_fn_();
    } catch (const FileError& error) {
      last_file_errno_.store(error.number, std::memory_order_relaxed);
      flush_failures_.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
      flush_failures_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  void complete_flushes(bool output_flushed) noexcept {
    std::lock_guard<std::mutex> lock(control_mutex_);
    for (auto it = flush_requests_.begin(); it != flush_requests_.end();) {
      FlushRequest& request = **it;
      if (!request.fence.ready()) {
        ++it;
        continue;
      }
      // Include requests registered after this pass's automatic-flush check.
      // The worker has delivered every ready fence before this flush attempt.
      if (!output_flushed) {
        flush_output();
        output_flushed = true;
      }
      it = flush_requests_.erase(it);
      // Notify under the request lock: its owner may destroy it upon waking.
      std::lock_guard<std::mutex> request_lock(request.mutex);
      request.complete = true;
      request.condition.notify_one();
    }
    controls_pending_.store(!flush_requests_.empty(), std::memory_order_release);
  }

  void reclaim_producers() {
    bool changed = false;
    for (const auto& producer : worker_producers_) {
      if (producer->closed.load(std::memory_order_acquire) &&
          producer->consumed.load(std::memory_order_relaxed) == producer->published.load(std::memory_order_acquire)) {
        changed = true;
        break;
      }
    }
    if (!changed) return;
    {
      std::lock_guard<std::mutex> lock(producers_mutex_);
      for (auto it = producers_.begin(); it != producers_.end();) {
        const auto& producer = *it;
        if (producer->closed.load(std::memory_order_acquire) &&
            producer->consumed.load(std::memory_order_relaxed) == producer->published.load(std::memory_order_acquire)) {
          retired_dropped_ += producer->dropped.load(std::memory_order_relaxed);
          it = producers_.erase(it);
        } else {
          ++it;
        }
      }
      producers_revision_.fetch_add(1, std::memory_order_release);
    }
    refresh_producers();
  }

  bool has_pending() const noexcept {
    if (controls_pending_.load(std::memory_order_acquire) ||
        worker_revision_ != producers_revision_.load(std::memory_order_acquire)) return true;
    for (const auto& producer : worker_producers_) {
      if (producer->consumed.load(std::memory_order_relaxed) != producer->published.load(std::memory_order_acquire)) return true;
      if (producer->closed.load(std::memory_order_acquire)) return true;
    }
    return false;
  }

  void wait_for_work() {
    auto timeout = idle_interval_;
    if (output_dirty_) {
      const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
          flush_deadline_ - std::chrono::steady_clock::now());
      timeout = std::min(timeout, std::max(std::chrono::milliseconds::zero(), remaining));
    }
    std::unique_lock<std::mutex> lock(signal_->mutex);
    signal_->sleeping.store(1, std::memory_order_release);
    std::atomic_thread_fence(std::memory_order_seq_cst);
    if (!stopping_.load(std::memory_order_acquire) && !has_pending()) {
      signal_->condition.wait_for(lock, timeout, [this] {
        return !signal_->sleeping.load(std::memory_order_acquire);
      });
    }
    signal_->sleeping.store(0, std::memory_order_release);
  }

  void run() noexcept {
    for (;;) {
      refresh_producers();
      const auto& state = sinks_;
      const auto sinks = std::atomic_load(&state->list);
      for (const auto& producer : worker_producers_) drain_producer(*producer, *sinks);
      bool output_flushed = false;
      if (output_dirty_ && (urgent_flush_ || std::chrono::steady_clock::now() >= flush_deadline_)) {
        flush_output();
        output_flushed = true;
      }
      for (const auto& sink : *sinks) {
        try {
          if (sink->drained) sink->drained();
        } catch (...) {
          sink_drain_failures_.fetch_add(1, std::memory_order_relaxed);
        }
      }
      complete_flushes(output_flushed);
      reclaim_producers();
      if (has_pending()) continue;
      if (stopping_.load(std::memory_order_acquire)) {
        flush_output();
        return;
      }
      wait_for_work();
    }
  }

  void vlogf_impl(LogLevel level, const char* tag, const char* fmt, va_list ap, uint64_t source_id = 0) noexcept {
    // Compile-time and runtime filter before doing any formatting work.
    if (!should_log(level)) return;
    char buf[SlotSize];
    std::size_t start = 0;
    if (tag && tag[0] != '\0') {
      const auto length = std::strlen(tag);
      if (length < SlotSize - 3) {
        buf[0] = '[';
        std::memcpy(buf + 1, tag, length);
        buf[length + 1] = ']';
        buf[length + 2] = ' ';
        start = length + 3;
      }
    }
    const int n = std::vsnprintf(buf + start, SlotSize - start, fmt, ap);
    const std::size_t total = (n > 0)
        ? std::min(start + static_cast<std::size_t>(n), SlotSize - 1)
        : start;
    if (total > 0) {
      log_impl(level, std::string_view(buf, total), nullptr, source_id,
               static_cast<uint16_t>(start >= 3 ? start - 3 : 0));
    }
  }

  void write_default(const Record& record) noexcept {
    static constexpr const char* levels[] = {"[V] ", "[D] ", "[I] ", "[W] ", "[E] ", "[?] "};
    char line[SlotSize];
    std::size_t offset = 0;
    if (with_timestamp_) {
      const auto seconds = std::chrono::system_clock::to_time_t(record.timestamp);
      const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                              record.timestamp.time_since_epoch()) %
                          1000000;
      thread_local time_t cached_second = static_cast<time_t>(-1);
      thread_local struct tm local{};
      if (cached_second != seconds) {
#ifdef _WIN32
        localtime_s(&local, &seconds);
#else
        localtime_r(&seconds, &local);
#endif
        cached_second = seconds;
      }
      const int size = std::snprintf(line, SlotSize, "[%04d-%02d-%02d %02d:%02d:%02d.%06lld] ",
                                     local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                                     local.tm_hour, local.tm_min, local.tm_sec, static_cast<long long>(micros.count()));
      if (size > 0 && static_cast<std::size_t>(size) < SlotSize) offset = size;
    }
    const char* level = levels[std::min(static_cast<uint8_t>(record.level), uint8_t{5})];
    const int size = std::snprintf(line + offset, SlotSize - offset, "%s", level);
    if (size > 0) offset = std::min(offset + static_cast<std::size_t>(size), SlotSize - 1);
    if (record.source_id) {
      const int count = std::snprintf(line + offset, SlotSize - offset, "[source=%llu] ",
                                      static_cast<unsigned long long>(record.source_id));
      if (count > 0) offset = std::min(offset + static_cast<std::size_t>(count), SlotSize - 1);
    }
    const auto length = std::min(record.message.size(), SlotSize - 1 - offset);
    if (length) std::memcpy(line + offset, record.message.data(), length);
    if (flush_fn_) {
      if (!output_dirty_) {
        output_dirty_ = true;
        flush_deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
      }
      urgent_flush_ = urgent_flush_ || record.level >= LogLevel::ERROR;
    }
    try {
      write_fn_(record.level, line, offset + length);
    } catch (const FileError& error) {
      last_file_errno_.store(error.number, std::memory_order_relaxed);
      write_failures_.fetch_add(1, std::memory_order_relaxed);
    } catch (...) {
      write_failures_.fetch_add(1, std::memory_order_relaxed);
    }
  }

  struct Sink {
    uint64_t id;
    RecordFn write;
    DrainFn drained;
  };
  struct Sinks {
    std::mutex mutex;
    uint64_t next_id{0};
    std::shared_ptr<const SinkList> list = std::make_shared<const SinkList>();
  };

  // ── Members ────────────────────────────────────────────────────────────────

  const std::shared_ptr<Sinks> sinks_ = std::make_shared<Sinks>();
  std::shared_ptr<const int> identity_ = std::make_shared<const int>(0);
  const std::shared_ptr<WakeSignal> signal_;
  mutable std::mutex producers_mutex_;
  std::vector<std::shared_ptr<Producer>> producers_;
  std::atomic<uint64_t> producers_revision_{0};
  uint64_t next_producer_id_ = 0;
  uint64_t retired_dropped_ = 0;
  std::atomic<uint64_t> allocation_dropped_{0};
  std::atomic<uint64_t> write_failures_{0};
  std::atomic<uint64_t> flush_failures_{0};
  std::atomic<uint64_t> sink_write_failures_{0};
  std::atomic<uint64_t> sink_drain_failures_{0};
  std::atomic<int> last_file_errno_{0};
  // Only the output worker accesses its cached registry and revision.
  std::vector<std::shared_ptr<Producer>> worker_producers_;
  uint64_t worker_revision_ = 0;
  alignas(64) WriteFn write_fn_;
  FlushFn                   flush_fn_;
  std::atomic<uint8_t>      min_level_;
  std::chrono::milliseconds idle_interval_;
  bool                      with_timestamp_;
  // Output-worker state, independent of idle waits and producer submission.
  bool output_dirty_ = false;
  bool urgent_flush_ = false;
  std::chrono::steady_clock::time_point flush_deadline_{};

  std::atomic<bool> stopping_{false};
  std::mutex control_mutex_;
  std::vector<FlushRequest*> flush_requests_;
  std::atomic<bool> controls_pending_{false};

  std::thread worker_;
};

}  // namespace agilink

#endif  // AGILINK_OMNILOGGER_H_

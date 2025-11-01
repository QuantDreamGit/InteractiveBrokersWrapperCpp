//
// Created by user on 10/23/25.
//

#ifndef QUANTDREAMCPP_LOGGER_H
#define QUANTDREAMCPP_LOGGER_H

#include <atomic>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

// --------------------------------------------------------------------------
// Macros for easy logging
// --------------------------------------------------------------------------
#define LOG_DEBUG(...)       Logger::debug(__VA_ARGS__)
#define LOG_TIMER(...)       Logger::timer(__VA_ARGS__)
#define LOG_INFO(...)        Logger::info(__VA_ARGS__)
#define LOG_STRATEGY(...)    Logger::strategy(__VA_ARGS__)
#define LOG_WARN(...)        Logger::warn(__VA_ARGS__)
#define LOG_ERROR(...)       Logger::error(__VA_ARGS__)
#define LOG_EMPTY()          Logger::empty()
#define LOG_SECTION(title)   Logger::section(title)
#define LOG_SECTION_END()    Logger::sectionEnd()

/**
 * @brief Thread-safe logger with multiple log levels, including TIMER and STRATEGY.
 *
 * Order of severity:
 *   DEBUG < TIMER < INFO < STRATEGY < WARN < ERROR < NONE
 */
class Logger {
public:
  enum class Level {
    DEBUG = 0,
    TIMER,
    INFO,
    STRATEGY,
    WARN,
    ERROR,
    NONE
  };

private:
  static inline std::atomic<bool> enabled{true};
  static inline std::mutex logMutex;
  static inline Level minLevel = Level::DEBUG;

  static const char* levelName(Level lvl) {
    switch (lvl) {
      case Level::DEBUG:    return "DEBUG";
      case Level::TIMER:    return "TIMER";
      case Level::INFO:     return "INFO";
      case Level::STRATEGY: return "STRATEGY";
      case Level::WARN:     return "WARN";
      case Level::ERROR:    return "ERROR";
      default:              return "";
    }
  }

public:
  static void setEnabled(bool on) { enabled = on; }
  static void setLevel(Level lvl) { minLevel = lvl; }

  template <typename... Args>
  static void log(Level lvl, Args&&... args) {
    if (!enabled || lvl < minLevel) return;
    std::lock_guard<std::mutex> lock(logMutex);

    std::ostringstream oss;
    ((oss << std::forward<Args>(args)), ...);
    std::cout << "[" << levelName(lvl) << "] " << oss.str() << std::endl;
  }

  // --------------------------------------------------------------------------
  // Convenience helpers
  // --------------------------------------------------------------------------
  template <typename... Args> static void debug(Args&&... args)    { log(Level::DEBUG,    std::forward<Args>(args)...); }
  template <typename... Args> static void timer(Args&&... args)    { log(Level::TIMER,    std::forward<Args>(args)...); }
  template <typename... Args> static void info(Args&&... args)     { log(Level::INFO,     std::forward<Args>(args)...); }
  template <typename... Args> static void strategy(Args&&... args) { log(Level::STRATEGY, std::forward<Args>(args)...); }
  template <typename... Args> static void warn(Args&&... args)     { log(Level::WARN,     std::forward<Args>(args)...); }
  template <typename... Args> static void error(Args&&... args)    { log(Level::ERROR,    std::forward<Args>(args)...); }

  // --------------------------------------------------------------------------
  // Utility: print a clean empty line (thread-safe)
  // --------------------------------------------------------------------------
  static void empty() {
    if (!enabled) return;
    std::lock_guard<std::mutex> lock(logMutex);
    std::cout << std::endl;
  }

  // --------------------------------------------------------------------------
  // Section helpers: banners for major steps
  // --------------------------------------------------------------------------
  static void section(const std::string& title,
                      Level lvl = Level::INFO,
                      size_t totalWidth = 70)
  {
    if (!enabled || lvl < minLevel) return;
    std::lock_guard<std::mutex> lock(logMutex);

    // Use Unicode dash if the terminal supports UTF-8, otherwise ASCII
#ifdef _WIN32
    const std::string dash = "-";
#else
    const std::string dash = "─";  // UTF-8, 3-byte character
#endif

    // Calculate dash counts based on title length
    size_t dashCount = (totalWidth > title.size() + 2)
                         ? (totalWidth - title.size() - 2) / 2
                         : 3;

    // Build left/right sides manually (avoid char literal overflow)
    std::string left, right;
    left.reserve(dashCount * dash.size());
    right.reserve(dashCount * dash.size());
    for (size_t i = 0; i < dashCount; ++i) {
      left += dash;
      right += dash;
    }

    std::ostringstream oss;
    oss << left << " " << title << " " << right;
    std::cout << "\n[" << levelName(lvl) << "] " << oss.str() << std::endl;
  }

  static void sectionEnd(Level lvl = Level::INFO,
                       size_t totalWidth = 70)
  {
    if (!enabled || lvl < minLevel) return;
    std::lock_guard<std::mutex> lock(logMutex);

#ifdef _WIN32
    const std::string dash = "-";
#else
    const std::string dash = "─";
#endif

    std::string line;
    line.reserve(totalWidth * dash.size());
    for (size_t i = 0; i < totalWidth; ++i)
      line += dash;

    std::cout << "[" << levelName(lvl) << "] " << line << std::endl;
  }
};

#endif  // QUANTDREAMCPP_LOGGER_H

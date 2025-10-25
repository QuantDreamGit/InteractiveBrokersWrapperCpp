//
// Created by user on 10/23/25.
//

#ifndef QUANTDREAMCPP_LOGGER_H
#define QUANTDREAMCPP_LOGGER_H
#include <atomic>
#include <iostream>
#include <mutex>
#include <sstream>

// --------------------------------------------------------------------------
// Macros for easy logging
// --------------------------------------------------------------------------
#define LOG_DEBUG(...)    Logger::debug(__VA_ARGS__)
#define LOG_TIMER(...)    Logger::timer(__VA_ARGS__)
#define LOG_INFO(...)     Logger::info(__VA_ARGS__)
#define LOG_STRATEGY(...) Logger::strategy(__VA_ARGS__)
#define LOG_WARN(...)     Logger::warn(__VA_ARGS__)
#define LOG_ERROR(...)    Logger::error(__VA_ARGS__)

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
};

#endif  // QUANTDREAMCPP_LOGGER_H

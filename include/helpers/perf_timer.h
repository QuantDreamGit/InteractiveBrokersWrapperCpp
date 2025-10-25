//
// Created by user on 10/23/25.
//

#ifndef QUANTDREAMCPP_PERF_TIMER_H
#define QUANTDREAMCPP_PERF_TIMER_H
#include <chrono>
#include <future>
#include <type_traits>
#include "logger.h"

namespace IB::Helpers {

  using Clock = std::chrono::high_resolution_clock;

  // --------------------------------------------------------------------------
  //  Measure synchronous callable
  // --------------------------------------------------------------------------

  /**
   * @brief Measure execution time of a callable, log duration, and return its result if any.
   */
  template <typename Func>
  auto measure(Func&& func, const std::string& label = "") {
    auto start = Clock::now();

    if constexpr (std::is_void_v<std::invoke_result_t<Func>>) {
      func();
      auto end = Clock::now();
      double ms = std::chrono::duration<double, std::milli>(end - start).count();
      LOG_TIMER("[PerfTimer] ", label.empty() ? "Function" : label, " took ", ms, " ms");
    } else {
      auto result = func();
      auto end = Clock::now();
      double ms = std::chrono::duration<double, std::milli>(end - start).count();
      LOG_TIMER("[PerfTimer] ", label.empty() ? "Function" : label, " took ", ms, " ms");
      return result;
    }
  }

  // --------------------------------------------------------------------------
  //  Measure an existing std::future<T>
  // --------------------------------------------------------------------------

  /**
   * @brief Measure and log how long it takes for a std::future<T> to complete.
   *        Returns its result if non-void.
   */
  template <typename T>
  auto measureFuture(std::future<T>& fut, const std::string& label = "") {
    auto start = Clock::now();

    if constexpr (std::is_void_v<T>) {
      fut.get();
      auto end = Clock::now();
      double ms = std::chrono::duration<double, std::milli>(end - start).count();
      LOG_TIMER("[PerfTimer] ", label.empty() ? "Future" : label, " resolved in ", ms, " ms");
    } else {
      T result = fut.get();
      auto end = Clock::now();
      double ms = std::chrono::duration<double, std::milli>(end - start).count();
      LOG_TIMER("[PerfTimer] ", label.empty() ? "Future" : label, " resolved in ", ms, " ms");
      return result;
    }
  }

  // --------------------------------------------------------------------------
  //  Measure async function returning std::future<T>
  // --------------------------------------------------------------------------

  /**
   * @brief Measure total duration of an async function (returns std::future<T>),
   *        waits for completion, logs duration, and returns result.
   */
  template <typename Func,
            typename FutureType = std::invoke_result_t<Func>,
            typename T = typename FutureType::value_type>
  auto measureAsync(Func&& func, const std::string& label = "") {
    auto start = Clock::now();

    FutureType fut = func();  // must return std::future<T>
    if constexpr (std::is_void_v<T>) {
      fut.get();
      auto end = Clock::now();
      double ms = std::chrono::duration<double, std::milli>(end - start).count();
      LOG_TIMER("[PerfTimer] ", label.empty() ? "Async call" : label, " completed in ", ms, " ms");
    } else {
      T result = fut.get();
      auto end = Clock::now();
      double ms = std::chrono::duration<double, std::milli>(end - start).count();
      LOG_TIMER("[PerfTimer] ", label.empty() ? "Async call" : label, " completed in ", ms, " ms");
      return result;
    }
  }

}  // namespace Helpers
#endif  // QUANTDREAMCPP_PERF_TIMER_H

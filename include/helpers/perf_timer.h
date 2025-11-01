//
// Created by user on 10/23/25.
//

#ifndef QUANTDREAMCPP_PERF_TIMER_H
#define QUANTDREAMCPP_PERF_TIMER_H
#include <chrono>
#include <future>
#include <type_traits>
#include "logger.h"

/**
 * @file perf_timer.h
 * @brief Performance measurement utilities for timing synchronous and asynchronous operations
 *
 * This file provides templated functions to measure execution time of callables, futures,
 * and async operations. It automatically handles both void and non-void return types,
 * logging execution duration while preserving return values.
 */

namespace IB::Helpers {

  using Clock = std::chrono::high_resolution_clock;  ///< High-precision clock for performance measurements

  // --------------------------------------------------------------------------
  //  Measure synchronous callable
  // --------------------------------------------------------------------------

  /**
   * @brief Measures execution time of a callable and logs the duration
   *
   * @tparam Func Callable type (function, lambda, functor)
   * @param func The callable to measure
   * @param label Optional label for log output (default: "Function")
   * @return The result of calling `func()` (if non-void), otherwise void
   *
   * This function performs high-precision timing of any callable:
   *
   * **For void callables:**
   * - Records start time
   * - Executes the callable
   * - Records end time and logs duration
   *
   * **For non-void callables:**
   * - Records start time
   * - Executes the callable and captures result
   * - Records end time and logs duration
   * - Returns the captured result
   *
   * **Implementation Details:**
   * - Uses `std::chrono::high_resolution_clock` for nanosecond precision
   * - Automatically detects return type via `std::invoke_result_t`
   * - Uses `if constexpr` for compile-time branching (C++17)
   * - Logs duration in milliseconds with `LOG_TIMER` macro
   *
   * @note The measurement includes only the callable's execution time, not overhead
   *       from timing infrastructure itself (typically < 1µs).
   *
   * @note For async operations or futures, use `measureAsync()` or `measureFuture()`
   *       instead to measure total completion time.
   *
   * Example usage:
   * @code
   * // Measure void function
   * IB::Helpers::measure([]() {
   *     processLargeDataset();
   * }, "Dataset Processing");
   * // Output: [PerfTimer] Dataset Processing took 1234.56 ms
   *
   * // Measure function with return value
   * auto result = IB::Helpers::measure([]() {
   *     return computeComplexCalculation();
   * }, "Complex Calculation");
   * // Output: [PerfTimer] Complex Calculation took 567.89 ms
   * // Returns: result from computeComplexCalculation()
   *
   * // Measure without label
   * IB::Helpers::measure([]() { quickOperation(); });
   * // Output: [PerfTimer] Function took 12.34 ms
   * @endcode
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
   * @brief Measures how long it takes for an existing future to complete
   *
   * @tparam T Future value type (can be void)
   * @param fut Reference to the future to measure (will be moved/consumed)
   * @param label Optional label for log output (default: "Future")
   * @return The result from `fut.get()` (if non-void), otherwise void
   *
   * This function measures the blocking wait time for a future to resolve:
   *
   * **Workflow:**
   * - Records start time
   * - Calls `fut.get()` which blocks until the future is ready
   * - Records end time and logs duration
   * - Returns the future's result (if non-void)
   *
   * **Use Cases:**
   * - Measuring network request latency
   * - Timing database query responses
   * - Profiling async computation completion
   * - Tracking IB API response times
   *
   * @note The future is consumed by this function (via `get()`). Do not reuse it afterward.
   *
   * @note The measurement includes only the wait time, not the original async operation's
   *       creation overhead. Total time = async work duration + wait overhead.
   *
   * @warning The calling thread will block until the future completes. For timeouts,
   *          use `fut.wait_for()` before calling this function.
   *
   * Example usage:
   * @code
   * // Measure future with void result
   * std::future<void> asyncTask = std::async(std::launch::async, []() {
   *     performAsyncWork();
   * });
   * IB::Helpers::measureFuture(asyncTask, "Async Work");
   * // Output: [PerfTimer] Async Work resolved in 2345.67 ms
   *
   * // Measure future with return value
   * std::future<int> computation = std::async(std::launch::async, []() {
   *     return expensiveComputation();
   * });
   * int result = IB::Helpers::measureFuture(computation, "Computation");
   * // Output: [PerfTimer] Computation resolved in 890.12 ms
   * // Returns: result from expensiveComputation()
   *
   * // Measure IB API future
   * auto priceFuture = ib.createPromise<double>(reqId);
   * double price = IB::Helpers::measureFuture(priceFuture, "Price Request");
   * // Output: [PerfTimer] Price Request resolved in 156.78 ms
   * @endcode
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
   * @brief Measures total execution time of an async function that returns a future
   *
   * @tparam Func Callable type that returns `std::future<T>`
   * @tparam FutureType Automatically deduced future type from Func's return type
   * @tparam T Automatically deduced value type of the future
   * @param func Callable that returns `std::future<T>` when invoked
   * @param label Optional label for log output (default: "Async call")
   * @return The result from the future's `get()` (if non-void), otherwise void
   *
   * This function measures the complete async operation from initiation to completion:
   *
   * **Workflow:**
   * - Records start time
   * - Invokes `func()` to get a `std::future<T>`
   * - Blocks on `fut.get()` until the async operation completes
   * - Records end time and logs total duration
   * - Returns the future's result (if non-void)
   *
   * **Measured Time Includes:**
   * - Function call overhead
   * - Async task setup and scheduling
   * - Actual async work execution
   * - Context switching and thread synchronization
   * - Future resolution and data transfer
   *
   * **Typical Use Cases:**
   * - Measuring `std::async()` wrapped operations
   * - Timing promise-based async workflows
   * - Profiling IB API async request patterns
   * - Benchmarking thread pool task submission
   *
   * @note This differs from `measureFuture()` by measuring the **total time** including
   *       async task creation, whereas `measureFuture()` only measures wait time.
   *
   * @note The function must return `std::future<T>`. For functions returning `std::shared_future<T>`
   *       or custom future types, convert or use `measureFuture()` instead.
   *
   * @warning The calling thread blocks until completion. For fire-and-forget async tasks,
   *          this function is not appropriate.
   *
   * Example usage:
   * @code
   * // Measure std::async operation with void result
   * IB::Helpers::measureAsync([]() {
   *     return std::async(std::launch::async, []() {
   *         performHeavyWork();
   *     });
   * }, "Heavy Work");
   * // Output: [PerfTimer] Heavy Work completed in 3456.78 ms
   *
   * // Measure async operation with return value
   * auto result = IB::Helpers::measureAsync([]() {
   *     return std::async(std::launch::async, []() {
   *         return fetchDataFromAPI();
   *     });
   * }, "API Fetch");
   * // Output: [PerfTimer] API Fetch completed in 1234.56 ms
   * // Returns: data from fetchDataFromAPI()
   *
   * // Measure IB API async request
   * auto greeks = IB::Helpers::measureAsync([&]() {
   *     return ib.getOptionGreeksAsync(contract);
   * }, "Greeks Request");
   * // Output: [PerfTimer] Greeks Request completed in 234.56 ms
   * @endcode
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

}  // namespace IB::Helpers
#endif  // QUANTDREAMCPP_PERF_TIMER_H

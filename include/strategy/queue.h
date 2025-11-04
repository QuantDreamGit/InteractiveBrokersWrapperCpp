#ifndef QUANTDREAMCPP_QUEUE_H
#define QUANTDREAMCPP_QUEUE_H

#include <condition_variable>
#include <mutex>
#include <queue>

/**
 * @brief Thread-safe concurrent blocking queue.
 *
 * This class provides a simple, generic, and thread-safe FIFO queue that allows
 * multiple producers and consumers to safely push and pop elements concurrently.
 * It uses a mutex and condition variable for synchronization.
 *
 * @tparam T Type of the elements stored in the queue.
 *
 * Typical usage example:
 * @code
 * ConcurrentQueue<MarketTick> q;
 *
 * // Producer thread
 * q.push(MarketTick{"AAPL", 190.32});
 *
 * // Consumer thread
 * auto tick = q.pop(); // Blocks until data is available
 * @endcode
 */
template<typename T>
class ConcurrentQueue {
public:
  /**
   * @brief Push an element into the queue.
   *
   * Adds an element to the queue in a thread-safe manner.
   * If one or more threads are blocked in @ref pop(), one will be notified.
   *
   * @param v The element to add. It will be moved into the queue.
   */
  void push(T v) {
    {
      std::lock_guard<std::mutex> lk(m_);
      q_.push(std::move(v));
    }
    cv_.notify_one();
  }

  /**
   * @brief Blocking pop operation.
   *
   * Waits until an element is available in the queue and returns it.
   * If the queue has been stopped and is empty, this function throws
   * a std::runtime_error to signal shutdown.
   *
   * @return The next element from the queue (moved out).
   * @throws std::runtime_error If the queue is stopped and empty.
   */
  T pop() {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [&]{ return !q_.empty() || stopped_; });
    if (stopped_ && q_.empty()) throw std::runtime_error("queue stopped");
    T v = std::move(q_.front());
    q_.pop();
    return v;
  }

  /**
   * @brief Stop the queue and wake up all waiting threads.
   *
   * Signals to all consumers waiting in @ref pop() that the queue
   * is shutting down. Once stopped, further calls to @ref pop()
   * will throw if the queue is empty.
   *
   * This is typically called during system shutdown to allow threads
   * to exit gracefully.
   */
  void stop() {
    {
      std::lock_guard<std::mutex> lk(m_);
      stopped_ = true;
    }
    cv_.notify_all();
  }

  /**
   * @brief Check whether the queue is empty.
   *
   * Thread-safe inspection of whether there are any elements in the queue.
   *
   * @return True if the queue is empty, false otherwise.
   */
  bool empty() const {
    std::lock_guard<std::mutex> lk(m_);
    return q_.empty();
  }

private:
  mutable std::mutex m_;               ///< Mutex protecting queue and state.
  std::condition_variable cv_;         ///< Condition variable for blocking waits.
  std::queue<T> q_;                    ///< Underlying standard queue container.
  bool stopped_{false};                ///< Flag indicating whether the queue is stopped.
};

#endif  // QUANTDREAMCPP_QUEUE_H

#ifndef QUANTDREAMCPP_ORDER_EXECUTION_H
#define QUANTDREAMCPP_ORDER_EXECUTION_H

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "Contract.h"
#include "Order.h"
#include "helpers/logger.h"
#include "strategy/queue.h"

/**
 * @brief Lightweight container for an order submission request.
 *
 * Combines a contract and order object into a single unit that can be
 * passed through message queues for asynchronous execution.
 */
struct OrderRequest {
  int localId;        ///< Optional local correlation ID for internal tracking.
  Contract contract;  ///< Contract definition (e.g., stock, option, future).
  Order order;        ///< Order parameters (side, limit, quantity, etc.).
};

/**
 * @brief Asynchronous order execution worker.
 *
 * The OrderExecutor consumes order requests from a concurrent queue in a
 * dedicated worker thread and invokes a provided execution function for each
 * request. This design decouples order generation (strategy logic) from
 * actual broker communication (execution).
 *
 * Example usage:
 * @code
 * auto orderQueue = std::make_shared<ConcurrentQueue<OrderRequest>>();
 *
 * // Define how to execute an order
 * OrderExecutor::ExecuteFn execFn = [&](OrderRequest&& r) {
 *     ibWrapper.placeOrder(r.localId, r.contract, r.order);
 * };
 *
 * // Create executor with background worker
 * OrderExecutor executor(orderQueue, execFn);
 *
 * // Push orders from strategy thread
 * orderQueue->push({1, contract, order});
 * @endcode
 */
class OrderExecutor {
public:
  /// Function signature for executing a single order request.
  using ExecuteFn = std::function<void(OrderRequest&&)>;

  /**
   * @brief Construct and start the order execution worker.
   *
   * Launches a background thread that continuously processes incoming
   * order requests from the provided queue.
   *
   * @param q Shared pointer to the concurrent order queue.
   * @param executor Function to execute each order request.
   */
  explicit OrderExecutor(std::shared_ptr<ConcurrentQueue<OrderRequest>> q,
                         ExecuteFn executor)
    : queue_(std::move(q)), execute_(std::move(executor)), running_(true)
  {
    worker_ = std::thread([this]{ run(); });
  }

  /**
   * @brief Destructor that stops the worker thread gracefully.
   *
   * Signals the worker loop to exit, stops the queue, and joins the
   * thread to ensure a clean shutdown.
   */
  ~OrderExecutor() {
    running_ = false;
    if (queue_) queue_->stop();
    if (worker_.joinable()) worker_.join();
  }

private:
  /**
   * @brief Worker thread loop.
   *
   * Continuously pops order requests from the queue and executes them
   * via the provided @ref ExecuteFn. Any exceptions thrown by the
   * executor function are caught and logged, preventing thread termination.
   */
  void run() const {
    try {
      while (running_) {
        auto req = queue_->pop();
        try {
          execute_(std::move(req));
        } catch (const std::exception& e) {
          LOG_ERROR("[OrderExecutor] execute error: ", e.what());
        }
      }
    } catch (const std::exception&) {
      // Queue stopped or shutting down
    }
  }

  std::shared_ptr<ConcurrentQueue<OrderRequest>> queue_;  ///< Shared pointer to the order queue.
  ExecuteFn execute_;                                     ///< Callback used to execute each order.
  std::atomic<bool> running_;                             ///< Flag controlling the worker loop.
  std::thread worker_;                                    ///< Background worker thread.
};

#endif  // QUANTDREAMCPP_ORDER_EXECUTION_H

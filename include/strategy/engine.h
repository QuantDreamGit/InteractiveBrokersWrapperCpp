#ifndef QUANTDREAMCPP_ENGINE_H
#define QUANTDREAMCPP_ENGINE_H

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "data_structures/snapshots.h"
#include "order_execution.h"
#include "strategy/queue.h"

/// Type alias for a market data snapshot (replace with your actual type).
using MarketSnapshot = IB::MarketData::MarketSnapshot;

/**
 * @brief Core strategy engine responsible for processing market data and generating orders.
 *
 * The StrategyEngine runs a dedicated worker thread that listens for incoming
 * market snapshots (via @ref onSnapshot) and applies trading logic to decide
 * whether to send orders. Generated orders are pushed to an outbound queue,
 * typically consumed by an @ref OrderExecutor.
 *
 * Typical usage:
 * @code
 * auto orderQueue = std::make_shared<ConcurrentQueue<OrderRequest>>();
 * StrategyEngine engine(orderQueue);
 *
 * // Feed market data
 * engine.onSnapshot(marketSnap);
 * @endcode
 */
class StrategyEngine {
public:
  /// Function signature for external snapshot handling (not used directly here, but can be extended).
  using SnapshotFn = std::function<void(const MarketSnapshot&)>;

  /**
   * @brief Construct and start the strategy engine.
   *
   * Launches a background worker thread that continuously polls the latest
   * market data snapshot and executes strategy logic.
   *
   * @param outQueue Shared pointer to an outbound order queue.
   */
  explicit StrategyEngine(std::shared_ptr<ConcurrentQueue<OrderRequest>> outQueue)
    : outQueue_(std::move(outQueue)), running_(true)
  {
    worker_ = std::thread([this]{ loop(); });
  }

  /**
   * @brief Destructor that stops the worker thread gracefully.
   *
   * Signals the strategy loop to stop and joins the thread to ensure
   * a clean shutdown.
   */
  ~StrategyEngine() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
  }

  /**
   * @brief Receive a new market snapshot.
   *
   * This function should be called by the market data handler or IB wrapper
   * whenever new market data is available. It safely updates the internal
   * snapshot buffer for processing by the strategy loop.
   *
   * @param snap Latest market snapshot data.
   */
  void onSnapshot(const MarketSnapshot& snap) {
    std::lock_guard<std::mutex> lk(inMutex_);
    latest_ = snap;
    newData_ = true;
  }

private:
  /**
   * @brief Internal worker loop that runs the trading strategy.
   *
   * The loop wakes periodically, checks for new market data, and applies
   * trading logic. When trade conditions are met, it constructs an
   * @ref OrderRequest and pushes it to the outbound queue.
   */
  void loop() {
    while (running_) {
      MarketSnapshot snap;
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> lk(inMutex_);
        if (!newData_) continue;
        snap = latest_;
        newData_ = false;
      }

      // === Example strategy logic ===
      // Replace this with your own trading rules.
      if (snap.last > 0) {
        OrderRequest req;
        req.localId = 0;
        // Fill Contract / Order according to your system design:
        // req.contract = buildContract(...);
        // req.order = buildMarketOrder("BUY", 1);
        outQueue_->push(std::move(req));
      }
    }
  }

  std::shared_ptr<ConcurrentQueue<OrderRequest>> outQueue_;  ///< Outgoing order queue shared with executor.
  std::atomic<bool> running_;                                ///< Flag controlling the main loop.
  std::thread worker_;                                       ///< Background thread executing the strategy.

  std::mutex inMutex_;     ///< Protects access to the latest market snapshot.
  MarketSnapshot latest_;  ///< Latest received market data snapshot.
  bool newData_{false};    ///< Indicates whether new data is available for processing.
};

#endif  // QUANTDREAMCPP_ENGINE_H

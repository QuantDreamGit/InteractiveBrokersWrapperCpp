#ifndef QUANTDREAMCPP_STRATEGY_BASE_H
#define QUANTDREAMCPP_STRATEGY_BASE_H

#include "data_structures/snapshots.h"

/// Type alias for a market data snapshot (replace with your actual IB type).
using MarketSnapshot = IB::MarketData::MarketSnapshot;

/**
 * @brief Abstract base class for all trading strategies.
 *
 * The StrategyBase class defines the common interface that every strategy
 * must implement. It provides standardized entry points for receiving market
 * data, starting, and stopping the strategy lifecycle.
 *
 * Derived classes implement their own logic for interpreting market data
 * and generating trading signals or orders.
 *
 * Typical usage:
 * @code
 * class MomentumStrategy : public StrategyBase {
 * public:
 *     void onSnapshot(const MarketSnapshot& snap) override {
 *         // Analyze market snapshot and make decisions
 *     }
 *
 *     void start() override {
 *         // Initialize state, indicators, etc.
 *     }
 *
 *     void stop() override {
 *         // Cleanup, close open positions, etc.
 *     }
 * };
 * @endcode
 */
class StrategyBase {
public:
  /**
   * @brief Called whenever a new market snapshot is available.
   *
   * Implement this function to process incoming market data and possibly
   * generate trade signals. The snapshot may include bid/ask prices, last
   * traded price, volume, and other relevant fields depending on your
   * market data source.
   *
   * @param snap The latest market snapshot.
   */
  virtual void onSnapshot(const MarketSnapshot& snap) = 0;

  /**
   * @brief Start the strategy.
   *
   * This is called to initialize strategy components and launch any internal
   * background threads or computations. Called once when the system starts.
   */
  virtual void start() = 0;

  /**
   * @brief Stop the strategy.
   *
   * Called to gracefully shut down the strategy, stop background loops,
   * and release any resources held. This may also include sending cancel
   * requests or flattening open positions if desired.
   */
  virtual void stop() = 0;

  /// Virtual destructor to allow safe polymorphic destruction.
  virtual ~StrategyBase() = default;
};

#endif  // QUANTDREAMCPP_STRATEGY_BASE_H

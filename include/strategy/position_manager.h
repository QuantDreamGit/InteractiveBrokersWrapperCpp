#ifndef QUANTDREAMCPP_POSITION_MANAGER_H
#define QUANTDREAMCPP_POSITION_MANAGER_H

#include <map>
#include <mutex>
#include <vector>
#include <functional>
#include "data_structures/positions.h"
#include "data_structures/snapshots.h"

/**
 * @brief Thread-safe manager for current portfolio positions and market data.
 *
 * The PositionManager stores and provides access to the current open positions
 * received from the Interactive Brokers API. It supports concurrent access from
 * multiple threads (e.g., IB callback threads and strategy threads).
 *
 * Additionally, it provides callback hooks for market data events (bid, ask, mid, last)
 * that can be registered to receive real-time price updates when they are computed.
 *
 * Typical usage pattern:
 * @code
 * PositionManager pm;
 *
 * // Register market data callbacks:
 * pm.setOnMidCallback([](int tickerId, double mid) {
 *     std::cout << "Mid price for " << tickerId << ": " << mid << std::endl;
 * });
 *
 * // Called from IB position callback:
 * pm.onPosition(positionInfo);
 *
 * // Called from IB positionEnd callback:
 * pm.onPositionClear();
 *
 * // Read current snapshot for strategy logic:
 * auto positions = pm.snapshot();
 * for (const auto& p : positions)
 *     std::cout << p.contract.symbol << " => " << p.position << std::endl;
 * @endcode
 */
class PositionManager {
public:
  /**
   * @brief Store or update a position entry.
   *
   * Called whenever the IB API reports a position via its `position()` callback.
   * Updates the stored position for the given contract ID.
   * Also triggers the registered onPositionCallback if set.
   *
   * @param p The position information received from IB.
   */
  void onPosition(const IB::Accounts::PositionInfo& p) {
    {
      std::lock_guard<std::mutex> lk(m_);
      positions_[p.contract.conId] = p;
    }
    
    // Call the registered callback if set (outside lock to avoid deadlocks)
    if (onPositionCallback_) {
      onPositionCallback_(p);
    }
  }

  /**
   * @brief Clear all stored positions.
   *
   * Typically called when the IB API sends a `positionEnd()` event or when
   * resetting state during reconnection or account refresh.
   */
  void onPositionClear() {
    std::lock_guard<std::mutex> lk(m_);
    positions_.clear();
  }

  /**
   * @brief Take a snapshot of all current positions.
   *
   * Creates and returns a copy of the current positions in a vector.
   * This allows the caller to safely inspect the current state of
   * positions without holding any locks.
   *
   * @return A vector of @ref IB::Accounts::PositionInfo representing
   *         all currently known positions.
   */
  std::vector<IB::Accounts::PositionInfo> snapshot() const {
    std::lock_guard<std::mutex> lk(m_);
    std::vector<IB::Accounts::PositionInfo> out;
    out.reserve(positions_.size());
    for (const auto& kv : positions_) out.push_back(kv.second);
    return out;
  }

  // =============================================================================
  // Market Data Callbacks - Called when IB market data events complete
  // =============================================================================

  /**
   * @brief Called when a bid price update completes.
   *
   * Invokes the registered onBid callback if one is set. This is typically
   * called from IBMarketWrapper::tickPrice() when a BID tick is received.
   *
   * @param tickerId The IB request ID for the market data
   * @param bid The current bid price
   */
  void onBid(int tickerId, double bid) {
    if (onBidCallback_) onBidCallback_(tickerId, bid);
  }

  /**
   * @brief Called when an ask price update completes.
   *
   * Invokes the registered onAsk callback if one is set. This is typically
   * called from IBMarketWrapper::tickPrice() when an ASK tick is received.
   *
   * @param tickerId The IB request ID for the market data
   * @param ask The current ask price
   */
  void onAsk(int tickerId, double ask) {
    if (onAskCallback_) onAskCallback_(tickerId, ask);
  }

  /**
   * @brief Called when a last trade price update completes.
   *
   * Invokes the registered onLast callback if one is set. This is typically
   * called from IBMarketWrapper::tickPrice() when a LAST tick is received.
   *
   * @param tickerId The IB request ID for the market data
   * @param last The last trade price
   */
  void onLast(int tickerId, double last) {
    if (onLastCallback_) onLastCallback_(tickerId, last);
  }

  /**
   * @brief Called when a mid price is computed (average of bid/ask).
   *
   * Invokes the registered onMid callback if one is set. This is typically
   * called from IBMarketWrapper when both bid and ask are available and
   * the mid price can be calculated.
   *
   * @param tickerId The IB request ID for the market data
   * @param mid The computed mid price (bid + ask) / 2
   */
  void onMid(int tickerId, double mid) {
    if (onMidCallback_) onMidCallback_(tickerId, mid);
  }

  /**
   * @brief Called when a complete market snapshot is ready.
   *
   * Invokes the registered onSnapshot callback if one is set. This is called
   * when all required market data fields have been received and the snapshot
   * is ready for strategy consumption.
   *
   * @param tickerId The IB request ID for the market data
   * @param snapshot The complete market snapshot with all available data
   */
  void onSnapshot(int tickerId, const IB::MarketData::MarketSnapshot& snapshot) {
    if (onSnapshotCallback_) onSnapshotCallback_(tickerId, snapshot);
  }

  // =============================================================================
  // Callback Registration - Set callbacks to receive market data events
  // =============================================================================

  /**
   * @brief Register a callback for bid price updates.
   *
   * @param callback Function to call when bid price is updated.
   *                 Signature: void(int tickerId, double bid)
   */
  void setOnBidCallback(std::function<void(int, double)> callback) {
    onBidCallback_ = std::move(callback);
  }

  /**
   * @brief Register a callback for ask price updates.
   *
   * @param callback Function to call when ask price is updated.
   *                 Signature: void(int tickerId, double ask)
   */
  void setOnAskCallback(std::function<void(int, double)> callback) {
    onAskCallback_ = std::move(callback);
  }

  /**
   * @brief Register a callback for last trade price updates.
   *
   * @param callback Function to call when last price is updated.
   *                 Signature: void(int tickerId, double last)
   */
  void setOnLastCallback(std::function<void(int, double)> callback) {
    onLastCallback_ = std::move(callback);
  }

  /**
   * @brief Register a callback for mid price updates.
   *
   * @param callback Function to call when mid price is computed.
   *                 Signature: void(int tickerId, double mid)
   */
  void setOnMidCallback(std::function<void(int, double)> callback) {
    onMidCallback_ = std::move(callback);
  }

  /**
   * @brief Register a callback for complete snapshot updates.
   *
   * @param callback Function to call when a complete snapshot is ready.
   *                 Signature: void(int tickerId, const MarketSnapshot& snap)
   */
  void setOnSnapshotCallback(std::function<void(int, const IB::MarketData::MarketSnapshot&)> callback) {
    onSnapshotCallback_ = std::move(callback);
  }

  /**
   * @brief Register a callback for position updates.
   *
   * @param callback Function to call when a position is received from IB.
   *                 Signature: void(const IB::Accounts::PositionInfo& position)
   */
  void setOnPositionCallback(std::function<void(const IB::Accounts::PositionInfo&)> callback) {
    onPositionCallback_ = std::move(callback);
  }

private:
  mutable std::mutex m_;  ///< Mutex protecting concurrent access to the positions map.
  std::map<int, IB::Accounts::PositionInfo> positions_;  ///< Map of positions keyed by IB contract ID.

  // Market data callbacks
  std::function<void(int, double)> onBidCallback_;
  std::function<void(int, double)> onAskCallback_;
  std::function<void(int, double)> onLastCallback_;
  std::function<void(int, double)> onMidCallback_;
  std::function<void(int, const IB::MarketData::MarketSnapshot&)> onSnapshotCallback_;
  
  // Position callback
  std::function<void(const IB::Accounts::PositionInfo&)> onPositionCallback_;
};

#endif  // QUANTDREAMCPP_POSITION_MANAGER_H

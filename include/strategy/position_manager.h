#ifndef QUANTDREAMCPP_POSITION_MANAGER_H
#define QUANTDREAMCPP_POSITION_MANAGER_H

#include <map>
#include <mutex>
#include <vector>
#include "data_structures/positions.h"

/**
 * @brief Thread-safe manager for current portfolio positions.
 *
 * The PositionManager stores and provides access to the current open positions
 * received from the Interactive Brokers API. It supports concurrent access from
 * multiple threads (e.g., IB callback threads and strategy threads).
 *
 * Typical usage pattern:
 * @code
 * PositionManager pm;
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
   *
   * @param p The position information received from IB.
   */
  void onPosition(const IB::Accounts::PositionInfo& p) {
    std::lock_guard<std::mutex> lk(m_);
    positions_[p.contract.conId] = p;
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

private:
  mutable std::mutex m_;  ///< Mutex protecting concurrent access to the positions map.
  std::map<int, IB::Accounts::PositionInfo> positions_;  ///< Map of positions keyed by IB contract ID.
};

#endif  // QUANTDREAMCPP_POSITION_MANAGER_H

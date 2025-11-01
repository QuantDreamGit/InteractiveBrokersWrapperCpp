#ifndef QUANTDREAMCPP_SNAPSHOTS_H
#define QUANTDREAMCPP_SNAPSHOTS_H

/**
 * @file snapshots.h
 * @brief Market data snapshot structures for Interactive Brokers API
 *
 * This file defines data structures for capturing market data snapshots from IB,
 * including price quotes (bid/ask/last) and option Greeks (delta, gamma, vega, theta).
 * It supports both one-time snapshots and continuous streaming modes with intelligent
 * fulfillment logic based on requested data types.
 */

namespace IB::MarketData {

/**
 * @brief Defines the type of price data requested in a market snapshot
 *
 * This enum controls which fields must be populated before a snapshot is considered
 * "fulfilled" and ready for use. Different strategies require different data completeness:
 *
 * - **LAST**: Only last trade price is required
 * - **BID**: Only bid price is required
 * - **ASK**: Only ask price is required
 * - **SNAPSHOT**: Default mode - requires both bid/ask quotes and Greeks (for options)
 * - **QUOTES_ONLY**: Only requires bid/ask prices, Greeks are optional
 * - **GREEKS_ONLY**: Fulfills only when Greeks data is available, quotes optional
 */
enum class PriceType {
  LAST,         ///< Fulfill when last trade price received
  BID,          ///< Fulfill when bid price received
  ASK,          ///< Fulfill when ask price received
  SNAPSHOT,     ///< Require both quotes + Greeks (default for options)
  QUOTES_ONLY,  ///< Only require bid/ask, ignore Greeks
  GREEKS_ONLY   ///< Allow fulfilling only when Greeks ready
};

/**
 * @brief Comprehensive market data snapshot with quotes, Greeks, and fulfillment state
 *
 * This structure captures all market data received from IB for a single contract, including
 * price quotes (bid/ask/last/OHLC), option model fields (Greeks), and metadata about
 * fulfillment status and streaming mode.
 *
 * **Data Categories:**
 *
 * **Quote Fields**
 * - Standard price data: bid, ask, last, open, close, high, low
 * - All initialized to 0.0 until IB sends updates
 *
 * **Option Model Fields (Greeks)**
 * - Greeks: delta, gamma, vega, theta, impliedVol
 * - Option price and underlying price
 * - `hasGreeks` flag indicates if Greeks data has been received
 *
 * **Meta Information**
 * - `mode`: Controls fulfillment logic via PriceType enum
 * - `fulfilled`: True when snapshot meets fulfillment criteria
 * - `cancelled`: True if market data request was cancelled
 * - `streaming`: False for one-time snapshots (auto-cancel), true for live streaming
 *
 * **Fulfillment Logic:**
 *
 * The `readyForFulfill()` method determines when a snapshot has sufficient data based on `mode`:
 *
 * - **LAST**: Fulfilled when `last > 0.0`
 * - **BID**: Fulfilled when `bid > 0.0`
 * - **ASK**: Fulfilled when `ask > 0.0`
 * - **QUOTES_ONLY**: Fulfilled when both `bid > 0.0` AND `ask > 0.0`
 * - **SNAPSHOT**: Fulfilled when quotes available (bid OR ask if Greeks present, bid AND ask otherwise)
 * - **GREEKS_ONLY**: Requires Greeks data
 *
 * @note For options, IB typically sends quotes first, then Greeks asynchronously. Use
 *       `SNAPSHOT` mode to ensure both are received before fulfilling.
 *
 * @note The `streaming` flag controls auto-cancellation behavior. Set to false for one-time
 *       snapshots that should cancel after fulfillment, true for continuous updates.
 *
 * Example usage:
 * @code
 * // Create snapshot request for option with Greeks
 * IB::MarketData::MarketSnapshot snapshot;
 * snapshot.mode = IB::MarketData::PriceType::SNAPSHOT;
 * snapshot.streaming = false;  // Auto-cancel after fulfillment
 *
 * // Check if snapshot is ready
 * if (snapshot.readyForFulfill() && !snapshot.fulfilled) {
 *     snapshot.fulfilled = true;
 *     std::cout << "Bid: " << snapshot.bid
 *               << " Ask: " << snapshot.ask
 *               << " IV: " << snapshot.impliedVol
 *               << " Delta: " << snapshot.delta << std::endl;
 * }
 *
 * // Create quotes-only snapshot (faster for non-options)
 * IB::MarketData::MarketSnapshot stockSnapshot;
 * stockSnapshot.mode = IB::MarketData::PriceType::QUOTES_ONLY;
 * stockSnapshot.streaming = true;  // Continuous updates
 * @endcode
 */
struct MarketSnapshot {
  // --- Quote fields ---
  double bid = 0.0;    ///< Current bid price
  double ask = 0.0;    ///< Current ask price
  double last = 0.0;   ///< Last trade price
  double open = 0.0;   ///< Opening price
  double close = 0.0;  ///< Closing price (previous day)
  double high = 0.0;   ///< Daily high price
  double low = 0.0;    ///< Daily low price

  // --- Option model fields (Greeks) ---
  double impliedVol = 0.0;  ///< Implied volatility (IV)
  double delta = 0.0;       ///< Delta (rate of change w.r.t. underlying)
  double gamma = 0.0;       ///< Gamma (rate of change of delta)
  double vega = 0.0;        ///< Vega (sensitivity to IV changes)
  double theta = 0.0;       ///< Theta (time decay)
  double optPrice = 0.0;    ///< Option theoretical price
  double undPrice = 0.0;    ///< Underlying asset price
  bool hasGreeks = false;   ///< True if Greeks data has been received

  // --- Meta info ---
  PriceType mode = PriceType::SNAPSHOT;   ///< Fulfillment mode (determines readiness criteria)
  bool fulfilled = false;                 ///< True when snapshot meets fulfillment criteria
  bool cancelled = false;                 ///< True if market data request was cancelled
  bool streaming = false;                 ///< False for snapshot (auto-cancel), true for live stream

  /**
   * @brief Checks if both bid and ask prices are available
   * @return True if both bid > 0 and ask > 0
   *
   * This is a quick validity check for quote data. Does not verify Greeks.
   */
  bool hasBidAsk() const noexcept { return bid > 0 && ask > 0; }

  /**
   * @brief Checks if Greeks data is valid and complete
   * @return True if hasGreeks flag is set, IV > 0, option price > 0, and delta != 0
   *
   * This validates that the snapshot contains meaningful Greeks data from IB.
   * A delta of exactly 0.0 typically indicates missing or invalid data.
   */
  bool hasGreeksData() const noexcept {
    return hasGreeks && impliedVol > 0 && optPrice > 0 && delta != 0.0;
  }

  /**
   * @brief Determines if the snapshot has sufficient data based on PriceType mode
   * @return True if the snapshot meets fulfillment criteria for its mode
   *
   * **Fulfillment Criteria by Mode:**
   *
   * - **LAST**: `last > 0.0`
   * - **BID**: `bid > 0.0`
   * - **ASK**: `ask > 0.0`
   * - **QUOTES_ONLY**: `(bid > 0.0) && (ask > 0.0)`
   * - **SNAPSHOT**: If Greeks received: `(bid > 0.0) || (ask > 0.0)`, otherwise: `(bid > 0.0) && (ask > 0.0)`
   * - **GREEKS_ONLY**: Requires `hasGreeks` flag (implementation depends on use case)
   *
   * This method should be called periodically as market data arrives to determine
   * when the snapshot is ready for use or cancellation.
   *
   * @note For SNAPSHOT mode, the logic relaxes bid/ask requirements if Greeks are present,
   *       allowing fulfillment with only one side of the quote.
   */
  bool readyForFulfill() const {
    switch (mode) {
      case PriceType::LAST:
        return last > 0.0;

      case PriceType::BID:
        return bid > 0.0;

      case PriceType::ASK:
        return ask > 0.0;

      case PriceType::QUOTES_ONLY:
        // If one side missing, still fulfill after BID or ASK appears
        return (bid > 0.0 & ask > 0.0);

      case PriceType::SNAPSHOT:
        // If Greeks requested, require both quotes + Greeks
        if (hasGreeks)
          return (bid > 0.0 || ask > 0.0);
        return (bid > 0.0 && ask > 0.0);

      default:
        return false;
    }
  }
};

} // namespace IB::MarketData

#endif // QUANTDREAMCPP_SNAPSHOTS_H

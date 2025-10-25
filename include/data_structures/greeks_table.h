//
// Created by user on 10/19/25.
//

#ifndef QUANTDREAMCPP_GREEKS_TABLE_H
#define QUANTDREAMCPP_GREEKS_TABLE_H
#include <string>

/**
 * @brief Represents the complete set of option Greeks and model data returned
 * by Interactive Brokers through the `EWrapper::tickOptionComputation()` callback.
 *
 * Each instance corresponds to a single option contract (identified by strike and right)
 * and captures the theoretical option model outputs such as implied volatility, delta,
 * gamma, vega, and theta, as well as the model price and underlying reference price.
 *
 * Fields:
 * - @b symbol        Underlying symbol (e.g., "AAPL").
 * - @b right         Option type: "C" for Call, "P" for Put.
 * - @b strike        Option strike price.
 * - @b expiry        Expiration date in IB format (e.g., "20251115").
 * - @b exchange      Exchange or routing venue where the option trades.
 * - @b tradingClass  Exchange trading class (root symbol).
 * - @b impliedVol    Model-calculated implied volatility (annualized, as a decimal).
 * - @b delta         Option Delta (∂Price/∂Underlying).
 * - @b gamma         Option Gamma (∂²Price/∂Underlying²).
 * - @b vega          Option Vega (∂Price/∂Volatility, per 1% change).
 * - @b theta         Option Theta (∂Price/∂Time, per day).
 * - @b optPrice      Theoretical option model price reported by IB.
 * - @b undPrice      Underlying price used in the model calculation.
 *
 * @note
 * IBKR provides these values in real-time or delayed mode, depending on the market
 * data type currently set via `reqMarketDataType()`. They can be used to build
 * delta tables, implied-volatility surfaces, or to select strikes for spreads
 * (e.g., iron condors or verticals).
 *
 * @see EWrapper::tickOptionComputation
 * @see EClient::reqMktData
 */
namespace IB::Options {

struct Greeks {
  std::string symbol;        ///< Underlying symbol.
  std::string right;         ///< "C" = Call, "P" = Put.
  double      strike = 0.0;  ///< Strike price.
  std::string expiry;        ///< Expiration date (YYYYMMDD).
  std::string exchange;      ///< Exchange or routing venue.
  std::string tradingClass;  ///< Trading class or root symbol.

  double impliedVol = 0.0;   ///< Implied volatility (annualized).
  double delta       = 0.0;  ///< Delta.
  double gamma       = 0.0;  ///< Gamma.
  double vega        = 0.0;  ///< Vega.
  double theta       = 0.0;  ///< Theta.
  double optPrice    = 0.0;  ///< Model (theoretical) option price.
  double undPrice    = 0.0;  ///< Underlying reference price used in the model.
};

} // namespace IB::Options
#endif  // QUANTDREAMCPP_GREEKS_TABLE_H

//
// Created by user on 10/17/25.
//

#ifndef QUANTDREAMCPP_OPTIONS_H
#define QUANTDREAMCPP_OPTIONS_H
#include <set>
#include <string>

namespace IB::Options {
  /**
   * @brief Represents the complete option chain definition for a given underlying contract.
   *
   * The OptionChain structure stores all metadata returned by the Interactive Brokers (IBKR) API
   * through the `securityDefinitionOptionParameter` callback. This metadata describes the available
   * options that can be traded for a specific underlying instrument (e.g., stock, index, or future).
   *
   * Each OptionChain instance corresponds to one combination of exchange and trading class,
   * containing all available expiration dates and strike prices for that class.
   *
   * Fields:
   * - @b exchange        The exchange that lists the options (e.g., "SMART", "CBOE", "GLOBEX").
   * - @b tradingClass    The trading class or option root symbol (e.g., "AAPL", "ES", "SPXW").
   * - @b multiplier      The contract multiplier as provided by IB (usually "100" for equity options).
   * - @b expirations     A sorted set of expiration dates (YYYY-MM-DD format) available for this class.
   * - @b strikes         A sorted set of all valid strike prices for the listed expirations.
   *
   * @note
   * This struct does not store market data or greeks—only the structural definition of the option chain.
   * To request live data for a specific option contract, create a `Contract` object using one of the
   * expiration dates, a strike price, and an option right ("C" or "P"), and pass it to `reqMktData()`.
   *
   * @see EWrapper::securityDefinitionOptionParameter
   * @see EClient::reqSecDefOptParams
   */
  struct ChainInfo {
    std::string exchange;
    std::string tradingClass;
    std::string multiplier;
    std::set<std::string> expirations;
    std::set<double> strikes;
  };

}
#endif  // QUANTDREAMCPP_OPTIONS_H

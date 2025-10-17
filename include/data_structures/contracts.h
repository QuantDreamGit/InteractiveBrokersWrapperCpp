//
// Created by user on 10/17/25.
//

#ifndef QUANTDREAMCPP_CONTRACTS_H
#define QUANTDREAMCPP_CONTRACTS_H
#include <string>
/**
 * @brief Represents the full details of a contract as returned by IB's `reqContractDetails()`
 * request.
 *
 * This structure holds all relevant information about a financial instrument (stock, option,
 * future, index, etc.) provided by the Interactive Brokers (IBKR) API through the
 * `EWrapper::contractDetails()` callback.
 *
 * Each instance corresponds to a single instrument and contains both the
 * general contract metadata and market-related attributes.
 *
 * Fields:
 * - @b conId             The unique contract identifier assigned by IBKR.
 * - @b symbol            The underlying symbol (e.g., "AAPL", "ES", "SPX").
 * - @b secType           The security type (e.g., "STK", "OPT", "FUT", "IND", "CFD", "CASH").
 * - @b exchange          The primary exchange or routing venue (e.g., "SMART", "CBOE", "GLOBEX").
 * - @b currency          The currency of the instrument (e.g., "USD", "EUR").
 * - @b lastTradeDateOrContractMonth
 *                        The last trading date or contract month (e.g., "20251017" or "202510").
 * - @b multiplier        The contract multiplier (e.g., "100" for equity options).
 * - @b tradingClass      The trading class or root symbol used on exchanges.
 * - @b marketName        The exchange's market name (IB-specific field).
 * - @b minTick           The minimum tick increment for price movements.
 * - @b validExchanges    A comma-separated list of all valid exchanges.
 *
 * @note
 * This struct represents *contract metadata only* — it does not include live market data, prices,
 * or positions. You can use the @b conId field from this struct to request further data, such as:
 * - Option chain definitions (`reqSecDefOptParams`)
 * - Market data (`reqMktData`)
 * - Historical data (`reqHistoricalData`)
 *
 * @see EWrapper::contractDetails
 * @see EClient::reqContractDetails
 */
namespace IB::Contracts {
  struct ContractInfo {
    long conId = 0;                           ///< Unique IBKR contract identifier.
    std::string symbol;                       ///< Underlying symbol.
    std::string secType;                      ///< Security type ("STK", "OPT", "FUT", etc.).
    std::string exchange;                     ///< Primary exchange or routing venue.
    std::string currency;                     ///< Contract currency.
    std::string lastTradeDateOrContractMonth; ///< Last trade date or contract month.
    std::string multiplier;                   ///< Contract multiplier (e.g., "100").
    std::string tradingClass;                 ///< Exchange trading class or root symbol.
    std::string marketName;                   ///< Market name from IB contract details.
    double minTick = 0.0;                     ///< Minimum tick size.
    std::string validExchanges;               ///< List of valid exchanges.
  };
}

#endif  // QUANTDREAMCPP_CONTRACTS_H

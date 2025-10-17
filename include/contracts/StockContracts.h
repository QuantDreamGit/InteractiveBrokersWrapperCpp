//
// Created by user on 10/15/25.
//

#ifndef QUANTDREAMCPP_STOCKCONTRACTS_H
#define QUANTDREAMCPP_STOCKCONTRACTS_H
#include <string>

#include "Contract.h"

namespace IB::Contracts {
  /**
   * Helper function to create a stock contract
   * @param symbol The stock symbol (e.g., "AAPL")
   * @param exchange The exchange to trade on (default: "SMART")
   * @param currency The currency of the stock (default: "USD")
   * @return A Contract object representing the stock
   */
  inline Contract makeStock(std::string const& symbol,
                            std::string const& exchange = "SMART",
                            std::string const& currency = "USD") {
    // Create a stock contract
    Contract contract;
    // Set the contract fields
    contract.symbol   = symbol;
    contract.secType  = "STK";
    contract.exchange = exchange;
    contract.currency = currency;
    return contract;
  }

  /**
 * Helper function to create an option contract
 * @param symbol The underlying stock symbol (e.g., "AAPL")
 * @param lastTradeDate The option's last trade date in YYYYMMDD format (e.g., "20231215")
 * @param strike The option's strike price (e.g., 150.0)
 * @param right The option type: "C" for Call, "P" for Put
 * @param multiplier The option multiplier (default: "100", typically 100 for stock options)
 * @param exchange The exchange to trade on (default: "SMART")
 * @param currency The currency of the option (default: "USD")
 */
  inline Contract makeOption(std::string const& symbol,
                             std::string const& lastTradeDate,
                             double const& strike,
                             std::string const& right,                // "C" for Call, "P" for Put
                             std::string const& multiplier = "100", // Typically 100 for stock options
                             std::string const& exchange = "SMART",
                             std::string const& currency = "USD") {
    // Create a stock contract
    Contract contract;
    // Set the contract fields
    contract.symbol   = symbol;
    contract.secType  = "OPT";
    contract.exchange = exchange;
    contract.currency = currency;
    contract.lastTradeDateOrContractMonth = lastTradeDate;
    contract.strike = strike;
    contract.right = right;
    contract.multiplier = multiplier;
    return contract;
  }
}
#endif  // QUANTDREAMCPP_STOCKCONTRACTS_H

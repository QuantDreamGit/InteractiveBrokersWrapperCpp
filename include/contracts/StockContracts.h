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

}
#endif  // QUANTDREAMCPP_STOCKCONTRACTS_H

//
// Created by user on 10/19/25.
//

#ifndef QUANTDREAMCPP_OPTIONCONTRACT_H
#define QUANTDREAMCPP_OPTIONCONTRACT_H

#include "Contract.h"
#include "request/contracts/ContractDetails.h"  // for getContractDetails
#include "wrappers/IBWrapperBase.h"
#include <string>

namespace IB::Contracts {

  /**
   * @brief Helper function to create and fully resolve an option contract via IB.
   *
   * If an active IB connection is provided, the function will automatically
   * call `reqContractDetails` to retrieve the fully qualified contract info
   * (including conId, localSymbol, and tradingClass).
   *
   * @param ib Optional IBWrapperBase reference — if provided, the function will fetch contract details.
   * @param symbol The underlying symbol (e.g., "AAPL").
   * @param expiration Expiration date (YYYYMMDD, e.g., "20251121").
   * @param strike The option strike price (e.g., 250.0).
   * @param right Option type ("C" for Call, "P" for Put).
   * @param exchange Exchange or routing destination (default: "SMART").
   * @param currency Option currency (default: "USD").
   * @param multiplier Contract multiplier (default: "100").
   * @param tradingClass Optional trading class (often same as symbol).
   * @param autoResolve If true (default), the contract will be enriched using IB contract details.
   * @return A fully populated Contract object.
   */
  inline Contract makeOption(const std::string& symbol,
                             const std::string& expiration,
                             const double strike,
                             const std::string& right,
                             const std::string& exchange = "SMART",
                             const std::string& currency = "USD",
                             const std::string& multiplier = "100",
                             const std::string& tradingClass = "",
                             IBWrapperBase* ib = nullptr,
                             bool autoResolve = true)
  {
    Contract c;
    c.symbol   = symbol;
    c.secType  = "OPT";
    c.currency = currency;
    c.exchange = exchange;
    c.lastTradeDateOrContractMonth = expiration;
    c.strike   = strike;
    c.right    = right;
    c.multiplier = multiplier;

    if (!tradingClass.empty())
      c.tradingClass = tradingClass;

    // --- Auto-resolve from IB if requested ---
    if (autoResolve && ib) {
      try {
        Contract resolved = IB::Requests::getContractDetails(*ib, c);
        if (resolved.conId != 0)
          return resolved;
      } catch (const std::exception& e) {
        LOG_WARN("[IB] makeOption: contract resolution failed for ",
                 symbol, " ", expiration, " ", strike, right,
                 " — reason: ", e.what());
      }
    }

    // fallback: return basic struct
    return c;
  }

}  // namespace IB::Contracts

#endif  // QUANTDREAMCPP_OPTIONCONTRACT_H

//
// Created by user on 10/29/25.
//

#ifndef QUANTDREAMCPP_LEGCONTRACT_H
#define QUANTDREAMCPP_LEGCONTRACT_H

#include "contracts/OptionContract.h"
#include "helpers/logger.h"
#include "wrappers/IBWrapperBase.h"

namespace IB::Contracts {
  /**
   * @brief Creates and resolves a single option leg (BUY/SELL CALL/PUT).
   *
   * Handles contract resolution, validation, and ComboLeg creation in one step.
   *
   * @param ib Reference to the active IBWrapperBase instance
   * @param underlyingSymbol e.g. "AAPL"
   * @param expiry Option expiry in format "YYYYMMDD"
   * @param strike Strike price (double)
   * @param right "C" for call, "P" for put
   * @param action "BUY" or "SELL"
   * @param exchange Exchange name (e.g. "SMART")
   * @param currency Currency (e.g. "USD")
   * @param multiplier Option multiplier, usually "100"
   * @param tradingClass IB option trading class (e.g. "AAPL")
   * @param legContracts [out] Vector collecting resolved leg contracts
   * @param legActions [out] Vector collecting leg actions in same order
   * @return ComboLeg IB combo leg structure
   */
  inline ComboLeg makeLeg(
      IBWrapperBase& ib,
      const std::string& underlyingSymbol,
      const std::string& expiry,
      double strike,
      const std::string& right,
      const std::string& action,
      const std::string& exchange,
      const std::string& currency,
      const std::string& multiplier,
      const std::string& tradingClass,
      std::vector<Contract>& legContracts,
      std::vector<std::string>& legActions)
  {
    // --- Create and resolve option contract ---
    Contract opt = IB::Contracts::makeOption(
        underlyingSymbol, expiry, strike, right, exchange,
        currency, multiplier, tradingClass, &ib, true);

    if (opt.conId == 0) {
      LOG_ERROR("[IB] makeLeg: Failed to resolve ", right,
                " strike=", strike, " ", underlyingSymbol, " exp=", expiry);
      throw std::runtime_error("Contract resolution failed");
    }

    // --- Store resolved leg metadata ---
    legContracts.push_back(opt);
    legActions.push_back(action);

    // --- Build combo leg ---
    ComboLeg leg;
    leg.conId = opt.conId;
    leg.ratio = 1;
    leg.action = action;
    leg.exchange = exchange;

    LOG_DEBUG("[IB] makeLeg created ", action, " ", right,
              " @ ", strike, " (conId=", leg.conId, ")");
    return leg;
  }
}

#endif  // QUANTDREAMCPP_LEGCONTRACT_H

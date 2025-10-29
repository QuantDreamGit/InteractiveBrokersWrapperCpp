//
// Created by user on 10/29/25.
//

#ifndef QUANTDREAMCPP_FAIR_PRICE_H
#define QUANTDREAMCPP_FAIR_PRICE_H

#include "get_prices.h"
#include "wrappers/IBWrapperBase.h"
#include "helpers/logger.h"

namespace IB::Options {

  /**
   * @brief Computes a fair midprice-based value for a multi-leg option combo.
   *
   * For each leg:
   *   - Requests a snapshot (bid/ask)
   *   - Computes mid = (bid + ask) / 2
   *   - Adds or subtracts mid based on BUY/SELL
   *
   * @return Fair combo value (positive = net credit, negative = net debit)
   */
  inline double computeFairPrice(
      IBWrapperBase& ib,
      const std::vector<Contract>& legs,
      const std::vector<std::string>& actions)
  {
    double fair = 0.0;
    int reqId = 5000;

    for (size_t i = 0; i < legs.size(); ++i) {
      const auto& leg = legs[i];
      const auto& act = actions[i];

      // --- Register contract metadata for callbacks ---
      ib.reqIdToContract[reqId] = leg;

      LOG_DEBUG("[IB] [FairPrice] Requesting market snapshot for ",
                leg.symbol, " ", leg.right, " strike=", leg.strike);

      // --- Get a one-time snapshot ---
      auto snap = IB::Requests::getSnapshot(ib, leg, reqId);

      // --- Compute midpoint ---
      double mid = (snap.bid + snap.ask) / 2.0;
      if (mid <= 0.0) {
        LOG_WARN("[IB] [FairPrice] Invalid midprice for ",
                 leg.symbol, " ", leg.right, " @", leg.strike);
        ++reqId;
        continue;
      }

      // --- Add or subtract mid based on leg direction ---
      fair += (act == "BUY" ? -mid : mid);

      LOG_INFO("[IB] [FairPrice] Leg ", i, " ",
               act, " ", leg.symbol, " ", leg.right,
               " strike=", leg.strike,
               " bid=", snap.bid, " ask=", snap.ask,
               " mid=", mid);

      ++reqId;
    }

    LOG_INFO("[IB] Computed fair price (midpoint-based) = ", fair);
    return fair;
  }

}  // namespace IB::Options

#endif  // QUANTDREAMCPP_FAIR_PRICE_H

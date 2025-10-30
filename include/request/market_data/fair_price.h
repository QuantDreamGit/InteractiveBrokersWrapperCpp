//
// Created by user on 10/29/25.
//

#ifndef QUANTDREAMCPP_FAIR_PRICE_H
#define QUANTDREAMCPP_FAIR_PRICE_H

#include "request/market_data/market_data.h"
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
    int reqId = IB::ReqId::SNAPSHOT_DATA_ID;

    for (size_t i = 0; i < legs.size(); ++i) {
      const auto& leg = legs[i];
      const auto& act = actions[i];

      LOG_DEBUG("[IB] [FairPrice] Requesting midprice for ",
                leg.symbol, " ", leg.right, " strike=", leg.strike);

      ib.reqIdToContract[reqId] = leg;
      double mid = IB::Requests::getMid(ib, leg, reqId);

      if (mid <= 0.0) {
        LOG_WARN("[IB] [FairPrice] Invalid midprice for ",
                 leg.symbol, " ", leg.right, " @", leg.strike);
        ++reqId;
        continue;
      }

      // BUY → negative (debit), SELL → positive (credit)
      fair += (act == "BUY" ? -mid : mid);

      LOG_INFO("[IB] [FairPrice] Leg ", i, " ",
               act, " ", leg.symbol, " ", leg.right,
               " strike=", leg.strike, " mid=", mid);

      ++reqId;
    }

    return fair;
  }


}  // namespace IB::Options

#endif  // QUANTDREAMCPP_FAIR_PRICE_H

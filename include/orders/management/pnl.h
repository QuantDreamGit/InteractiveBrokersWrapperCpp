//
// Created by user on 10/30/25.
//

#ifndef QUANTDREAMCPP_PNL_H
#define QUANTDREAMCPP_PNL_H
#include <iomanip>

#include "data_structures/positions.h"
#include "position.h"
#include "wrappers/IBBaseWrapper.h"

namespace IB::Orders::Management {
  template <typename T>
  requires std::is_base_of_v<IBBaseWrapper, T>
  inline void showCurrentPnL(T& ib) {
    LOG_SECTION("Current PnL Overview");

    // 1. Fetch open positions synchronously
    auto positions = IB::Orders::Management::getOpenPositions(ib);
    if (positions.empty()) {
      LOG_INFO("[IB] No open positions to evaluate PnL.");
      LOG_SECTION_END();
      return;
    }

    double totalPnL = 0.0;

    // 2. Loop through each position
    for (auto& p : positions) {
      Contract contract = p.contract; // copy

      // ✅ Ensure required fields
      if (contract.exchange.empty())
        contract.exchange = "SMART";
      if (contract.currency.empty())
        contract.currency = "USD";

      int reqId = ib.nextOrderId();
      auto snap = IBBaseWrapper::getSync<IB::MarketData::MarketSnapshot>(
          ib,
          reqId,
          [&]() {
              ib.client->reqMktData(reqId, contract, "", true, false, nullptr);
              ib.reqIdToContract[reqId] = contract;
              ib.snapshotData[reqId] = IB::MarketData::MarketSnapshot();
          }
      );

      double mark = snap.last;
      if (mark <= 0.0 && snap.hasBidAsk())
        mark = (snap.bid + snap.ask) / 2.0;
      else if (mark <= 0.0)
        mark = p.avgCost;

      double pnl = (mark - p.avgCost) * p.position;
      totalPnL += pnl;

      LOG_INFO("[PnL] ", contract.symbol, " ", contract.secType,
               " ", (p.position > 0 ? "LONG" : "SHORT"),
               " @ ", p.avgCost,
               " | Mark=", mark,
               " | PnL=", pnl);
    }

    LOG_INFO("[Total Unrealized PnL] ", std::fixed, std::setprecision(2), totalPnL, " USD");
    LOG_SECTION_END();
  }

} // namespace IB::Helpers
#endif  // QUANTDREAMCPP_PNL_H

//
// Created by user on 10/30/25.
//

#ifndef QUANTDREAMCPP_POSITION_H
#define QUANTDREAMCPP_POSITION_H
#include <vector>

#include "IBRequestIds.h"
#include "Order.h"
#include "data_structures/positions.h"
#include "wrappers/IBAccountWrapper.h"
#include "wrappers/IBBaseWrapper.h"

namespace IB::Orders::Management {
  template <typename T>
  requires std::is_base_of_v<IBBaseWrapper, T>
  inline std::vector<IB::Accounts::PositionInfo> getOpenPositions(T& ib) {
    int reqId = IB::ReqId::POSITION_ID; // you can define POSITION_ID = 9000 in IBRequestIds.h

    // Use getSync to wait until positionEnd fulfills the promise
    auto positions = IBBaseWrapper::getSync<std::vector<IB::Accounts::PositionInfo>>(
        ib,
        reqId,
        [&]() { ib.client->reqPositions(); }
    );

    // Log results
    LOG_INFO("[IB] Retrieved ", positions.size(), " open position(s).");
    for (auto& p : positions) {
      LOG_INFO("   ", p.contract.symbol, " ", p.contract.secType,
               " ", (p.position > 0 ? "LONG " : "SHORT "),
               std::abs(p.position),
               " @ avgCost=", p.avgCost);
    }

    return positions;
  }

  template <typename T>
  requires std::is_base_of_v<IBBaseWrapper, T>
  inline void closeAllPositions(T& ib) {
    LOG_SECTION("Closing All Open Positions");

    // 1. Fetch current open positions
    auto positions = IB::Orders::Management::getOpenPositions(ib);

    if (positions.empty()) {
      LOG_INFO("[IB] No open positions to close.");
      LOG_SECTION_END();
      return;
    }

    // Optional: cancel all pending orders before closing
    IB::Orders::Management::Open::cancelAll(ib);

    for (auto& p : positions) {
      double qty = std::abs(p.position);
      if (qty < 1e-6) continue; // skip zero

      // 2. Skip fractional positions
      if (std::fabs(qty - std::round(qty)) > 1e-6) {
        LOG_WARN("[IB] Skipping fractional position for ",
                 p.contract.symbol, " ", p.contract.secType,
                 " (qty=", qty, ")");
        continue;
      }

      // 3. Prepare contract copy (ensure exchange/currency)
      Contract contract = p.contract;
      if (contract.exchange.empty())
        contract.exchange = "SMART";
      if (contract.currency.empty())
        contract.currency = "USD";

      // 4. Create opposite market order
      Order order;
      order.orderType = "MKT";
      order.action = (p.position > 0 ? "SELL" : "BUY");
      order.totalQuantity = DecimalFunctions::doubleToDecimal(qty);

      int orderId = ib.nextOrderId();
      ib.client->placeOrder(orderId, contract, order);

      LOG_INFO("[IB] Closing position: ", contract.symbol,
               " ", contract.secType,
               " | Action=", order.action,
               " | Qty=", DecimalFunctions::decimalToDouble(order.totalQuantity),
               " | AvgCost=", p.avgCost);
    }

    LOG_SECTION_END();
  }

}

#endif  // QUANTDREAMCPP_POSITION_H

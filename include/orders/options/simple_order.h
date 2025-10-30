//
// Created by user on 10/24/25.
//

#ifndef QUANTDREAMCPP_SIMPLE_ORDER_H
#define QUANTDREAMCPP_SIMPLE_ORDER_H

#include "Order.h"
#include "contracts/OptionContract.h"
#include "helpers/logger.h"
#include "helpers/perf_timer.h"
#include "wrappers/IBBaseWrapper.h"

namespace IB::Orders::Options {

  /**
   * @brief Place a simple option order using the first available strike/expiry from the chain.
   *
   * Automatically resolves the contract and places an order.
   * Typically used for testing or quick order placement.
   */
  inline void placeSimpleOrder(
      IBBaseWrapper& ib,
      const Contract& underlying,
      const IB::Options::ChainInfo& chain,
      const Order& order,
      const std::string& right = "C")
  {
    IB::Helpers::measure([&]() {
      if (chain.expirations.empty() || chain.strikes.empty()) {
        LOG_ERROR("[IB] Option chain is empty — cannot place order.");
        return;
      }

      // --- Select first available expiration and strike ---
      const std::string expiry = *chain.expirations.begin();
      const double strike = *chain.strikes.begin();

      LOG_INFO("[IB] Using option ", right, " ", underlying.symbol,
               " exp=", expiry, " strike=", strike,
               " exch=", chain.exchange);

      // --- Build and auto-resolve the option contract ---
      Contract opt = IB::Contracts::makeOption(
          underlying.symbol,
          expiry,
          strike,
          right,
          chain.exchange.empty() ? "SMART" : chain.exchange,
          underlying.currency.empty() ? "USD" : underlying.currency,
          chain.multiplier.empty() ? "100" : chain.multiplier,
          chain.tradingClass,
          &ib,               // auto-resolve
          true               // enable getContractDetails()
      );

      if (opt.conId == 0) {
        LOG_ERROR("[IB] Failed to resolve option contract — aborting order.");
        return;
      }

      // --- Place the order ---
      const int orderId = ib.nextValidOrderId++; // track IDs internally
      ib.client->placeOrder(orderId, opt, order);

      LOG_INFO("[IB] Sent order #", orderId, " → ",
               order.action, " ", opt.localSymbol,
               " @ ", (order.orderType == "LMT" ? std::to_string(order.lmtPrice) : order.orderType));

    }, "placeSimpleOrder");
  }

}  // namespace IB::Orders::Options

#endif  // QUANTDREAMCPP_SIMPLE_ORDER_H

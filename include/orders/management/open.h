//
// Created by user on 10/24/25.
//

#ifndef QUANTDREAMCPP_OPEN_H
#define QUANTDREAMCPP_OPEN_H

#include "OrderCancel.h"
#include "helpers/logger.h"
#include "helpers/perf_timer.h"
#include "wrappers/IBBaseWrapper.h"

namespace IB::Orders::Management::Open {

  /**
   * @brief Request all open orders for this client only (non-blocking).
   */
  inline void requestClientOpenOrders(const IBBaseWrapper& ib) {
    IB::Helpers::measure([&]() {
      LOG_INFO("[IB] Requesting open orders for this client...");
      ib.client->reqOpenOrders();
    }, "requestClientOpenOrders");
  }

  /**
   * @brief Request all open orders for all clients across all API connections.
   */
  inline void requestAllOpenOrders(const IBBaseWrapper& ib) {
    IB::Helpers::measure([&]() {
      LOG_INFO("[IB] Requesting all open orders (across all clients)...");
      ib.client->reqAllOpenOrders();
    }, "requestAllOpenOrders");
  }

  /**
   * @brief Enables or disables automatic open order updates from TWS.
   */
  inline void subscribeAutoOpenOrders(const IBBaseWrapper& ib, bool enable = true) {
    IB::Helpers::measure([&]() {
      LOG_INFO("[IB] Setting auto-open order subscription: ", enable);
      ib.client->reqAutoOpenOrders(enable);
    }, "subscribeAutoOpenOrders");
  }

  /**
   * @brief Cancels a specific open order by ID.
   */
  inline void cancel(const IBBaseWrapper& ib, int orderId) {
    IB::Helpers::measure([&]() {
      LOG_INFO("[IB] Cancelling order #", orderId);
      OrderCancel cancelParams;
      cancelParams.manualOrderCancelTime = "";
      cancelParams.extOperator = "";
      cancelParams.manualOrderIndicator = UNSET_INTEGER;
      ib.client->cancelOrder(orderId, cancelParams);
    }, "cancelOrder");
  }

  /**
   * @brief Cancels all open orders globally for the account.
   */
  inline void cancelAll(const IBBaseWrapper& ib) {
    IB::Helpers::measure([&]() {
      LOG_SECTION("Global Cancel of All Open Orders");
      LOG_WARN("[IB] Sending global cancel — ALL open orders will be cancelled!");
      OrderCancel cancelParams;
      cancelParams.extOperator = "";
      cancelParams.manualOrderIndicator = UNSET_INTEGER;
      ib.client->reqGlobalCancel(cancelParams);
      LOG_SECTION_END();
    }, "cancelAllOrders");
  }

}  // namespace IB::Orders::Management::Open

#endif  // QUANTDREAMCPP_OPEN_H

//
// Created by user on 10/24/25.
//

#ifndef QUANTDREAMCPP_OPEN_ORDERS_H
#define QUANTDREAMCPP_OPEN_ORDERS_H
#include "Contract.h"
#include "OrderState.h"

namespace IB::Orders {
  /**
   * @brief Represents an open order with its full context (Contract, Order, and OrderState).
   */
  struct OpenOrdersInfo {
    int orderId = 0;               ///< IB-assigned order ID.
    Contract contract;             ///< Associated financial instrument.
    Order order;                   ///< The actual order specification.
    OrderState orderState;         ///< IB-reported state (Submitted, Filled, Cancelled, etc.).
  };
}
#endif  // QUANTDREAMCPP_OPEN_ORDERS_H

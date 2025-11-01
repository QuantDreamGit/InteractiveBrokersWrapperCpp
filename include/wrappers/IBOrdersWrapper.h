#ifndef QUANTDREAMCPP_IBORDERSWRAPPER_H
#define QUANTDREAMCPP_IBORDERSWRAPPER_H

#include "IBBaseWrapper.h"
#include "data_structures/open_orders.h"

/**
 * @file IBOrdersWrapper.h
 * @brief Order management and lifecycle tracking for Interactive Brokers API
 *
 * This file provides functionality for managing orders, tracking order status updates,
 * and handling open orders from Interactive Brokers TWS. It includes callback mechanisms
 * for real-time order event notifications.
 */

/**
 * @class IBOrdersWrapper
 * @brief Manages order lifecycle and open order tracking.
 *
 * Extends IBBaseWrapper to provide specialized handling for order-related callbacks
 * including order status updates, open orders list, and execution tracking. Provides
 * both buffered storage and callback-based notification mechanisms.
 */
class IBOrdersWrapper : public virtual IBBaseWrapper {
protected:
  /// Buffer storing current snapshot of open orders
  std::vector<IB::Orders::OpenOrdersInfo> openOrdersBuffer;

  /// Mutex for thread-safe access to open orders buffer
  std::mutex openOrdersMutex;

public:
  /// Callback invoked when a new open order is received
  std::function<void(const IB::Orders::OpenOrdersInfo&)> onOpenOrder;

  /// Callback invoked when all open orders have been transmitted
  std::function<void()> onOpenOrdersComplete;

  /**
   * @brief Retrieves a copy of the current open orders buffer
   *
   * @return Thread-safe copy of all currently buffered open orders
   *
   * This method is thread-safe and returns a snapshot of the open orders
   * at the time of the call. The buffer is cleared after each openOrderEnd() callback.
   */
  std::vector<IB::Orders::OpenOrdersInfo> getOpenOrders() {
    std::lock_guard<std::mutex> lock(openOrdersMutex);
    return openOrdersBuffer;
  }

  /**
   * @brief Callback invoked when order status changes
   *
   * @param orderId Unique identifier for the order
   * @param status Current status string (e.g., "Submitted", "Filled", "Cancelled")
   * @param filled Quantity that has been filled
   * @param remaining Quantity still remaining to be filled
   * @param avgFillPrice Average price of all fills so far
   * @param permId Permanent order ID assigned by TWS
   * @param parentId Parent order ID for bracket/combo orders (0 if none)
   * @param lastFillPrice Price of the most recent fill
   * @param clientId Client ID that placed the order
   * @param whyHeld Reason the order is held (empty if not held)
   * @param mktCapPrice Market capitalization price
   *
   * Logs order status updates including fill information. Ignores updates received
   * during initialization phase to avoid processing stale data from previous sessions.
   */
  void orderStatus(OrderId orderId, const std::string& status, Decimal filled,
                   Decimal remaining, double avgFillPrice, long long permId,
                   int parentId, double lastFillPrice, int clientId,
                   const std::string& whyHeld, double mktCapPrice) override {
    if (initializing) return;
    LOG_INFO("[OrderStatus] #", orderId, " ", status,
             " Filled=", DecimalFunctions::decimalToDouble(filled),
             " Remaining=", DecimalFunctions::decimalToDouble(remaining),
             " AvgPrice=", avgFillPrice);
  }

  /**
   * @brief Callback invoked for each open order during reqOpenOrders() request
   *
   * @param orderId Unique identifier for the open order
   * @param contract Contract details of the order
   * @param order Order details including action, quantity, type, etc.
   * @param orderState Current state of the order including status and commission
   *
   * Creates an OpenOrdersInfo object and adds it to the buffer in a thread-safe manner.
   * Also triggers the onOpenOrder callback if registered, allowing for immediate
   * processing of each order as it arrives. Ignores orders received during initialization
   * to prevent processing stale data.
   */
  void openOrder(OrderId orderId, const Contract& contract,
                 const Order& order, const OrderState& orderState) override {
    if (initializing) return;
    IB::Orders::OpenOrdersInfo info{(int)orderId, contract, order, orderState};
    {
      std::lock_guard<std::mutex> lock(openOrdersMutex);
      openOrdersBuffer.push_back(info);
    }
    if (onOpenOrder) onOpenOrder(info);
  }

  /**
   * @brief Callback invoked when all open orders have been transmitted
   *
   * Triggers the onOpenOrdersComplete callback if registered, signaling that
   * the complete snapshot of open orders is now available in the buffer.
   * Clears the buffer in a thread-safe manner to prepare for the next request.
   * This callback marks the end of a reqOpenOrders() or reqAllOpenOrders() sequence.
   */
  void openOrderEnd() override {
    if (onOpenOrdersComplete) onOpenOrdersComplete();
    std::lock_guard<std::mutex> lock(openOrdersMutex);
    openOrdersBuffer.clear();
  }
};

#endif

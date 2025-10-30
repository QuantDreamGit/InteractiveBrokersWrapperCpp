#ifndef QUANTDREAMCPP_IBORDERSWRAPPER_H
#define QUANTDREAMCPP_IBORDERSWRAPPER_H

#include "IBBaseWrapper.h"
#include "data_structures/open_orders.h"

/**
 * @class IBOrdersWrapper
 * @brief Manages order lifecycle and open order tracking.
 */
class IBOrdersWrapper : public virtual IBBaseWrapper {
protected:
  std::vector<IB::Orders::OpenOrdersInfo> openOrdersBuffer;
  std::mutex openOrdersMutex;

public:
  std::function<void(const IB::Orders::OpenOrdersInfo&)> onOpenOrder;
  std::function<void()> onOpenOrdersComplete;

  std::vector<IB::Orders::OpenOrdersInfo> getOpenOrders() {
    std::lock_guard<std::mutex> lock(openOrdersMutex);
    return openOrdersBuffer;
  }

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

  void openOrderEnd() override {
    if (onOpenOrdersComplete) onOpenOrdersComplete();
    std::lock_guard<std::mutex> lock(openOrdersMutex);
    openOrdersBuffer.clear();
  }
};

#endif

#ifndef QUANTDREAMCPP_IBSTRATEGYWRAPPER_H
#define QUANTDREAMCPP_IBSTRATEGYWRAPPER_H

#include "IBOrdersWrapper.h"
#include "IBMarketWrapper.h"
#include "IBAccountWrapper.h"

/**
 * @class IBStrategyWrapper
 * @brief Combines all wrapper functionality for strategy development.
 */
class IBStrategyWrapper :
  public virtual IBOrdersWrapper,
  public virtual IBMarketWrapper,
  public virtual IBAccountWrapper {
public:
  using IBBaseWrapper::connect;
  using IBBaseWrapper::disconnect;
  using IBBaseWrapper::nextOrderId;

  IBStrategyWrapper() {
    client = std::make_unique<EClientSocket>(this, &signal);
    LOG_INFO("[IBStrategyWrapper] EClientSocket bound to most derived object");
  }

  ~IBStrategyWrapper() override = default;

  void orderStatus(OrderId orderId, const std::string& status, Decimal filled,
                   Decimal remaining, double avgFillPrice, long long permId,
                   int parentId, double lastFillPrice, int clientId,
                   const std::string& whyHeld, double mktCapPrice) override {
    IBOrdersWrapper::orderStatus(orderId, status, filled, remaining,
                                 avgFillPrice, permId, parentId,
                                 lastFillPrice, clientId, whyHeld, mktCapPrice);
    LOG_INFO("[Strategy] Custom strategy-level order status handling");
  }
};

#endif

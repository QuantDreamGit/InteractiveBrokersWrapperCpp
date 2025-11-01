#ifndef QUANTDREAMCPP_IBSTRATEGYWRAPPER_H
#define QUANTDREAMCPP_IBSTRATEGYWRAPPER_H

#include "IBOrdersWrapper.h"
#include "IBMarketWrapper.h"
#include "IBAccountWrapper.h"

/**
 * @file IBStrategyWrapper.h
 * @brief Unified wrapper combining all IB API functionality for trading strategy development
 *
 * This file provides a complete wrapper that combines order management, market data,
 * and account handling capabilities in a single interface. It resolves diamond inheritance
 * from multiple base classes and ensures proper polymorphic behavior through explicit
 * client socket binding to the most derived object.
 */

/**
 * @class IBStrategyWrapper
 * @brief Unified interface combining orders, market data, and account management.
 *
 * Inherits virtually from IBOrdersWrapper, IBMarketWrapper, and IBAccountWrapper to provide
 * a complete API for trading strategy implementation. This is the recommended wrapper
 * for strategies that need full market access, position tracking, and order management.
 *
 * Uses virtual inheritance to resolve the diamond problem arising from multiple base
 * classes all inheriting from IBBaseWrapper. Rebinds the EClientSocket in the constructor
 * to ensure callbacks are routed to the most derived object.
 */
class IBStrategyWrapper :
  public virtual IBOrdersWrapper,
  public virtual IBMarketWrapper,
  public virtual IBAccountWrapper {
public:
  /// Import connect method from IBBaseWrapper to avoid ambiguity
  using IBBaseWrapper::connect;

  /// Import disconnect method from IBBaseWrapper to avoid ambiguity
  using IBBaseWrapper::disconnect;

  /// Import nextOrderId method from IBBaseWrapper to avoid ambiguity
  using IBBaseWrapper::nextOrderId;

  /**
   * @brief Constructor that binds EClientSocket to the most derived object
   *
   * Recreates the EClientSocket with `this` pointer pointing to IBStrategyWrapper
   * rather than any of the base classes. This ensures proper polymorphic callback
   * routing when multiple inheritance is involved, preventing callbacks from being
   * routed to incomplete base class instances.
   *
   * The signal object is inherited from IBBaseWrapper and shared across all bases.
   */
  IBStrategyWrapper() {
    client = std::make_unique<EClientSocket>(this, &signal);
    LOG_INFO("[IBStrategyWrapper] EClientSocket bound to most derived object");
  }

  /**
   * @brief Virtual destructor for proper cleanup
   *
   * Default destructor ensures base class destructors are called in proper order.
   * IBBaseWrapper::disconnect() is called automatically during destruction.
   */
  ~IBStrategyWrapper() override = default;

  /**
   * @brief Custom order status handler with strategy-level logging
   *
   * @param orderId Unique identifier for the order
   * @param status Current order status string (e.g., "Submitted", "Filled")
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
   * Overrides the base IBOrdersWrapper::orderStatus() to add strategy-specific
   * handling. First delegates to the base implementation for standard logging,
   * then adds custom strategy-level processing. This pattern allows strategies
   * to layer additional logic on top of standard order tracking.
   */
  void orderStatus(OrderId orderId, const std::string& status, Decimal filled,
                   Decimal remaining, double avgFillPrice, long long permId,
                   int parentId, double lastFillPrice, int clientId,
                   const std::string& whyHeld, double mktCapPrice) override {
    // Delegate to base class for standard order tracking
    IBOrdersWrapper::orderStatus(orderId, status, filled, remaining,
                                 avgFillPrice, permId, parentId,
                                 lastFillPrice, clientId, whyHeld, mktCapPrice);

    // Add strategy-specific handling (placeholder for custom logic)
    LOG_INFO("[Strategy] Custom strategy-level order status handling");
  }
};

#endif

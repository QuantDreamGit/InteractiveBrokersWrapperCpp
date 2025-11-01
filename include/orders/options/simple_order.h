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

/**
 * @file simple_order.h
 * @brief Simple option order placement for Interactive Brokers API
 *
 * This file provides a streamlined function for placing option orders using the first
 * available strike and expiration from a chain. Designed for testing, prototyping, or
 * scenarios where quick order placement is needed without manual contract selection.
 */

namespace IB::Orders::Options {

/**
 * @brief Places a simple option order using the first available strike and expiration from the chain
 *
 * @param ib Reference to the IBBaseWrapper instance (must be connected to IB Gateway/TWS)
 * @param underlying Contract specification for the underlying asset (e.g., stock, index)
 * @param chain ChainInfo containing available strikes, expirations, and exchange details
 * @param order Order object specifying action, quantity, type, price, and other parameters
 * @param right Option right - "C" for calls, "P" for puts (default: "C")
 *
 * This function performs a simplified option order placement workflow:
 *
 * **Step 1: Chain Validation**
 * - Checks if the chain contains at least one expiration and strike
 * - Aborts and logs error if chain is empty
 * - Ensures basic data availability before proceeding
 *
 * **Step 2: Automatic Selection**
 * - Selects the first available expiration from the chain (earliest date)
 * - Selects the first available strike from the chain (typically lowest strike)
 * - Logs the selected expiration, strike, and exchange for verification
 * - No filtering or analysis is performed - uses raw chain data
 *
 * **Step 3: Contract Construction and Resolution**
 * - Creates option contract using makeOption() with selected parameters
 * - Automatically resolves contract details to populate conId and localSymbol
 * - Uses chain exchange, currency, and multiplier with fallback defaults
 * - Validates resolution succeeded (conId != 0) before proceeding
 *
 * **Step 4: Order Submission**
 * - Generates next order ID from internal counter
 * - Submits order to IB Gateway/TWS via placeOrder()
 * - Logs order ID, action, symbol, and limit price (if applicable)
 * - Performance is automatically measured via "placeSimpleOrder" label
 *
 * @note This function is designed for simplicity, not optimization. It does not analyze
 *       Greeks, liquidity, or market conditions. Use specialized functions for production
 *       trading strategies.
 *
 * @note The function blocks briefly during contract resolution but returns immediately
 *       after order submission. Order status updates are handled via separate callbacks.
 *
 * @note The first strike in the chain may be far out-of-the-money or illiquid. For
 *       production use, consider filtering strikes by proximity to current price or
 *       volume metrics.
 *
 * @warning The chain must be populated before calling this function. An empty chain
 *          results in early return without placing an order.
 *
 * @warning The function uses internal order ID tracking via `nextValidOrderId`. Ensure
 *          this counter is properly initialized and synchronized if using multiple threads.
 *
 * @warning No validation is performed on the provided Order object. Invalid parameters
 *          (e.g., negative quantities, malformed limit prices) will be rejected by IB
 *          Gateway/TWS after submission.
 *
 * Example usage:
 * @code
 * // Get option chain
 * auto chain = IB::Request::getOptionChain(ib, underlying);
 *
 * // Create a limit order
 * Order buyOrder;
 * buyOrder.action = "BUY";
 * buyOrder.totalQuantity = 10;
 * buyOrder.orderType = "LMT";
 * buyOrder.lmtPrice = 2.50;
 * buyOrder.tif = "DAY";
 *
 * // Place simple call order (uses first expiration and strike)
 * IB::Orders::Options::placeSimpleOrder(
 *     ib,
 *     underlying,
 *     chain,
 *     buyOrder,
 *     "C"  // Calls
 * );
 *
 * // Place simple put order with market order type
 * Order sellOrder;
 * sellOrder.action = "SELL";
 * sellOrder.totalQuantity = 5;
 * sellOrder.orderType = "MKT";
 *
 * IB::Orders::Options::placeSimpleOrder(
 *     ib,
 *     underlying,
 *     chain,
 *     sellOrder,
 *     "P"  // Puts
 * );
 * @endcode
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

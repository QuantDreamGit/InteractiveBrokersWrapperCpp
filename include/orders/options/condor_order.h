//
// Created by user on 10/29/25.
//

#ifndef QUANTDREAMCPP_CONDOR_ORDER_H
#define QUANTDREAMCPP_CONDOR_ORDER_H

#include <algorithm>
#include <array>
#include <vector>

#include "Order.h"
#include "contracts/LegContract.h"
#include "contracts/OptionContract.h"
#include "helpers/logger.h"
#include "helpers/perf_timer.h"
#include "request/market_data/fair_price.h"
#include "wrappers/IBBaseWrapper.h"

/**
 * @file condor_order.h
 * @brief Iron Condor option strategy order placement for Interactive Brokers API
 *
 * This file provides functionality to construct and place Iron Condor orders, a popular
 * neutral options strategy consisting of four legs (two puts and two calls). The implementation
 * handles automatic strike selection, fair price computation from live market data, and
 * intelligent order routing with adaptive algorithms.
 */

namespace IB::Orders::Options {

/**
 * @brief Places an Iron Condor option strategy order with automatic strike selection and fair pricing
 *
 * @param ib Reference to the IBBaseWrapper instance (must be connected to IB Gateway/TWS)
 * @param underlying Contract specification for the underlying asset (e.g., stock, index)
 * @param chain ChainInfo containing available strikes, expirations, and exchange details
 * @param expiry Expiration date in "YYYYMMDD" format (e.g., "20251031")
 * @param strikes Array of 4 strikes [putBuy, putSell, callSell, callBuy]. If all zeros, auto-selects middle strikes
 * @param totalQuantity Number of Iron Condor spreads to trade (default: 1)
 * @param isBuy Direction - true for buy (debit spread), false for sell (credit spread) (default: true)
 * @param margin Price adjustment from fair value - positive increases execution probability (default: 0.10)
 * @param autoStrikes Enable automatic strike selection when strikes array is all zeros (default: false)
 *
 * This function constructs and submits a complete Iron Condor order with the following workflow:
 *
 * **Step 1: Strike Selection**
 * - If `strikes` is all zeros and `autoStrikes` is true, automatically selects 4 middle strikes from the chain
 * - Extracts and sorts available strikes from the chain
 * - Selects strikes around the middle price point for a balanced condor
 * - Logs selected strikes for verification
 * - If `strikes` is provided, uses those values after sorting them
 * - Throws exception if no strikes provided and auto-selection disabled
 *
 * **Step 2: Leg Construction**
 * - Constructs 4 option legs in the proper Iron Condor structure:
 *   - **Leg 1**: Buy 1 Put at lowest strike (downside protection)
 *   - **Leg 2**: Sell 1 Put at next higher strike (short put spread)
 *   - **Leg 3**: Sell 1 Call at next higher strike (short call spread)
 *   - **Leg 4**: Buy 1 Call at highest strike (upside protection)
 * - Resolves each option contract to populate conId and localSymbol
 * - Uses provided exchange, currency, and multiplier from chain or defaults
 * - Reverses actions if selling condor (credit spread)
 *
 * **Step 3: BAG Contract Creation**
 * - Creates a combo (BAG) contract to hold all 4 legs
 * - Converts legs to shared_ptr format required by IB API
 * - Associates legs with the combo contract
 *
 * **Step 4: Fair Price Computation**
 * - Dynamically computes fair price from current bid/ask spreads of all legs
 * - Uses `computeFairPrice()` to aggregate leg mid-prices
 * - Applies margin adjustment based on buy/sell direction:
 *   - **Buy**: `limitPrice = fairPrice - margin` (pay less)
 *   - **Sell**: `limitPrice = fairPrice + margin` (receive more)
 * - Rounds price to nearest tick (0.05) and ensures minimum of 0.01
 *
 * **Step 5: Order Submission**
 * - Creates Adaptive limit order for intelligent routing
 * - Sets order parameters: action, quantity, limit price, time-in-force (DAY)
 * - Submits order to IB Gateway/TWS via placeOrder()
 * - Logs order ID and execution parameters
 *
 * @note The function performs performance measurement and logs execution time via "placeIronCondor" label
 *
 * @note Iron Condor profit/loss characteristics:
 *       - **Max Profit**: Net premium received (for credit spread)
 *       - **Max Loss**: Width of widest spread minus net premium
 *       - **Breakeven**: Strike ± net premium on each side
 *
 * @note The Adaptive algorithm dynamically routes orders to minimize market impact and
 *       improve fill rates. It may split orders across multiple exchanges.
 *
 * @warning Requires valid market data subscriptions for all 4 option legs. Missing data
 *          will cause fair price computation to fail or use stale prices.
 *
 * @warning Auto-strike selection requires at least 4 strikes in the chain. Insufficient
 *          strikes will result in an error.
 *
 * @warning The function does not validate margin requirements or account permissions.
 *          Ensure your account has sufficient buying power and options approval level.
 *
 * Example usage:
 * @code
 * // Get option chain
 * auto chain = IB::Request::getOptionChain(ib, underlying);
 *
 * // Sell Iron Condor with auto-selected strikes (credit spread)
 * IB::Orders::Options::placeIronCondor(
 *     ib,
 *     underlying,
 *     chain,
 *     "20251115",    // Expiration
 *     {0,0,0,0},     // Auto-select strikes
 *     10,            // 10 contracts
 *     false,         // Sell (credit)
 *     0.15,          // 15% margin for better fill
 *     true           // Enable auto-strikes
 * );
 *
 * // Buy Iron Condor with specific strikes (debit spread)
 * IB::Orders::Options::placeIronCondor(
 *     ib,
 *     underlying,
 *     chain,
 *     "20251115",
 *     {95.0, 100.0, 110.0, 115.0},  // Specific strikes
 *     5,                             // 5 contracts
 *     true,                          // Buy (debit)
 *     0.10                           // 10% margin
 * );
 * @endcode
 */
inline void placeIronCondor(
    IBBaseWrapper& ib,
    const Contract& underlying,
    const IB::Options::ChainInfo& chain,
    const std::string& expiry,
    std::array<double,4> strikes = {0,0,0,0},
    int totalQuantity = 1,
    bool isBuy = true,
    double margin = 0.10,
    bool autoStrikes = false)
{
  auto roundToTick = [](double price, double tick = 0.05) {
    return std::round(price / tick) * tick;
  };

  IB::Helpers::measure([&]() {
    LOG_SECTION("Iron Condor Order Placement");
    std::array<double,4> usedStrikes{};

    // --- Step 1. Auto-select middle strikes if none provided ---
    if (std::all_of(strikes.begin(), strikes.end(), [](double s){ return s == 0.0; })) {
      // Be sure that auto strikes are requested intentionally
      if (autoStrikes) {
        // Extract and sort available strikes
        // They should be already sorted but for safety we sort them again
        std::vector<double> sorted(chain.strikes.begin(), chain.strikes.end());
        std::sort(sorted.begin(), sorted.end());

        if (sorted.size() < 4) {
            LOG_ERROR("[IB] Not enough strikes in chain to build Iron Condor.");
            return;
        }

        size_t mid = sorted.size() / 2;
        size_t start = (mid < 2) ? 0 : mid - 2;
        // Ensure we don't go out of bounds
        if (start + 4 > sorted.size()) start = sorted.size() - 4;

        // Select 4 middle strikes
        usedStrikes = { sorted[start], sorted[start+1], sorted[start+2], sorted[start+3] };
        LOG_INFO("[IB] Auto-selected middle strikes: ",
                 usedStrikes[0], ", ", usedStrikes[1], ", ",
                 usedStrikes[2], ", ", usedStrikes[3]);
      } else {
        LOG_WARN("No strike is provided for Iron Condor, and auto_strikes is disabled.");
        throw std::runtime_error("Strikes must be provided for Iron Condor.");
      }

    // Otherwise, use provided strikes
    } else {
        usedStrikes = strikes;
        std::sort(usedStrikes.begin(), usedStrikes.end());
    }

    // Construct leg strikes
    const double putBuy   = usedStrikes[0];
    const double putSell  = usedStrikes[1];
    const double callSell = usedStrikes[2];
    const double callBuy  = usedStrikes[3];

    LOG_INFO("[IB] Building Iron Condor on ", underlying.symbol,
             " exp=", expiry,
             " strikes=[", putBuy, ", ", putSell, ", ", callSell, ", ", callBuy, "]");

    // Be sure to set exchange, currency, and multiplier
    // Since data typically arrives from option chain func they are typically filled
    const std::string exch = chain.exchange.empty() ? "SMART" : chain.exchange;
    const std::string cur  = underlying.currency.empty() ? "USD" : underlying.currency;
    const std::string mult = chain.multiplier.empty() ? "100" : chain.multiplier;

    // --- Step 2. Helper to create and resolve each option leg ---
    std::vector<Contract> legContracts;   // Store Contracts
    std::vector<std::string> legActions;  // Store BUY, SELL (used in fair price computation)
    std::vector<ComboLeg> legs;           // Store Legs

    legs.push_back(IB::Contracts::makeLeg(ib, underlying.symbol, expiry, putBuy,  "P",
                           isBuy ? "BUY"  : "SELL", exch, cur, mult,
                           chain.tradingClass, legContracts, legActions));

    legs.push_back(IB::Contracts::makeLeg(ib, underlying.symbol, expiry, putSell, "P",
                           isBuy ? "SELL" : "BUY", exch, cur, mult,
                           chain.tradingClass, legContracts, legActions));

    legs.push_back(IB::Contracts::makeLeg(ib, underlying.symbol, expiry, callSell,"C",
                           isBuy ? "SELL" : "BUY", exch, cur, mult,
                           chain.tradingClass, legContracts, legActions));

    legs.push_back(IB::Contracts::makeLeg(ib, underlying.symbol, expiry, callBuy, "C",
                           isBuy ? "BUY"  : "SELL", exch, cur, mult,
                           chain.tradingClass, legContracts, legActions));

    // --- Step 3. Create combo (BAG) contract ---
    Contract combo;
    combo.symbol   = underlying.symbol;
    combo.secType  = "BAG";
    combo.currency = cur;
    combo.exchange = exch;

    // Legs must be added as pointers
    std::vector<std::shared_ptr<ComboLeg>> legPtrs;
    for (auto& leg : legs)
        legPtrs.push_back(std::make_shared<ComboLeg>(std::move(leg)));
    combo.comboLegs = std::make_shared<std::vector<std::shared_ptr<ComboLeg>>>(std::move(legPtrs));

    LOG_INFO("[IB] Built combo (BAG) contract with ", combo.comboLegs->size(), " legs.");

    // --- Step 4. Compute fair price dynamically from bid/ask mids ---
    double fairPrice = IB::Options::computeFairPrice(ib, legContracts, legActions);

    // Set margin if provided
    double limit = isBuy ? fairPrice - margin : fairPrice + margin;
    limit = roundToTick(std::max(0.01, limit), 0.05);

    // --- Step 5. Create Adaptive limit order ---
    Order comboOrder;
    comboOrder.action         = isBuy ? "BUY" : "SELL";
    comboOrder.orderType      = "LMT";
    comboOrder.totalQuantity  = totalQuantity;
    comboOrder.lmtPrice       = limit;
    comboOrder.tif            = "DAY";
    comboOrder.algoStrategy   = "Adaptive";

    LOG_INFO("[IB] Sending Condor at limit=", comboOrder.lmtPrice);

    // Place order
    const int orderId = ib.nextOrderId();
    ib.client->placeOrder(orderId, combo, comboOrder);

    LOG_INFO("[IB] Sent Adaptive Iron Condor order #", orderId,
             " (", comboOrder.action, " ", totalQuantity, "x ",
             underlying.symbol, " Condor, expiry=", expiry,
             ", limit=", fairPrice, ")");

    LOG_SECTION_END();
  }, "placeIronCondor");
}

} // namespace IB::Orders::Options

#endif  // QUANTDREAMCPP_CONDOR_ORDER_H

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

namespace IB::Orders::Options {

/**
 * @brief Places an Iron Condor order consisting of 4 option legs (2 calls, 2 puts).
 *
 * Legs are constructed as:
 *   - Buy 1 Put at lowest strike
 *   - Sell 1 Put at next higher strike
 *   - Sell 1 Call at next higher strike
 *   - Buy 1 Call at highest strike
 *
 * If no strikes are provided, automatically selects 4 middle strikes from the chain.
 *
 * @param ib The active IBWrapperBase instance (must be connected)
 * @param underlying The underlying contract (e.g. AAPL stock)
 * @param chain The option chain info
 * @param expiry The expiration date (e.g. "20251031")
 * @param strikes Optional array of 4 strikes. If empty, 4 middle strikes from the chain are used.
 * @param totalQuantity The number of spreads
 * @param isBuy Whether to buy or sell the condor (true = buy condor = debit, false = sell condor = credit)
 * @param margin Optional margin to have an execution price that is higher/lower than fairPrice
 * @param autoStrikes Whether to auto-select strikes if none provided, added for clarity.
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

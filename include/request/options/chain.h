//
// Created by user on 10/17/25.
//

#ifndef QUANTDREAMCPP_OPTIONCHAIN_H
#define QUANTDREAMCPP_OPTIONCHAIN_H

#include "IBRequestIds.h"
#include "request/contracts/ContractDetails.h"
#include "request/market_data/get_prices.h"
#include "wrappers/IBWrapperBase.h"

namespace IB::Request {

  /**
   * @brief Retrieve and filter the option chain for a given underlying.
   *
   * This version automatically:
   *  - Resolves the underlying contract (to get conId)
   *  - Requests all exchanges via reqSecDefOptParams
   *  - Filters strikes within ±strikeRangePct of the current price
   *  - Returns either the preferred exchange or SMART/first available
   */
  inline IB::Options::ChainInfo getOptionChain(
      IBWrapperBase& ib,
      const Contract& underlying,
      int reqId = IB::ReqId::OPTION_CHAIN_ID,
      double strikeRangePct = 0.25,
      const std::string& preferredExchange = "") {
    // Run perf timer
    return IB::Helpers::measure([&]() -> IB::Options::ChainInfo {

      // --- Step 1: Resolve the underlying contract (ensure conId is valid) ---
      Contract resolved = IB::Requests::getContractDetails(ib, underlying);
      if (resolved.conId == 0) {
        LOG_ERROR("[IB] getOptionChain: Unable to resolve underlying contract for ", underlying.symbol);
        return {};
      }

      // --- Step 2: Request option chain definitions ---
      auto allChains = IBWrapperBase::getSync<std::vector<IB::Options::ChainInfo>>(ib, reqId, [&]() {
        ib.client->reqSecDefOptParams(reqId,
                                      resolved.symbol,
                                      "",
                                      resolved.secType,
                                      static_cast<int>(resolved.conId));
      });

      LOG_DEBUG("[IB] Received option chain definitions for ", resolved.symbol,
                " (", allChains.size(), " exchanges)");

      if (allChains.empty()) {
        LOG_WARN("[IB] No option chain returned for ", resolved.symbol);
        return {};
      }

      // --- Step 3: Get underlying last price ---
      double lastPrice = IB::Requests::getLast(ib, resolved);
      if (lastPrice <= 0) {
        LOG_WARN("[IB] Could not retrieve valid price for ", resolved.symbol);
        return {};
      }

      const double lower = lastPrice * (1.0 - strikeRangePct);
      const double upper = lastPrice * (1.0 + strikeRangePct);
      LOG_DEBUG("[IB] Filtering strikes between ", lower, " and ", upper,
                " (±", strikeRangePct * 100, "% around ", lastPrice, ")");

      // --- Step 4: Filter strikes in range ---
      for (auto& chain : allChains) {
        std::set<double> filtered;
        for (double s : chain.strikes)
          if (s >= lower && s <= upper)
            filtered.insert(s);
        chain.strikes.swap(filtered);
      }

      // --- Step 5: Return preferred exchange if available ---
      auto findByExchange = [&](const std::string& exch) {
        return std::find_if(allChains.begin(), allChains.end(),
                            [&](const auto& c) { return c.exchange == exch; });
      };

      if (!preferredExchange.empty()) {
        auto it = findByExchange(preferredExchange);
        if (it != allChains.end()) {
          LOG_DEBUG("[IB] Using preferred exchange: ", preferredExchange);
          return *it;
        }
        LOG_WARN("[IB] Preferred exchange '", preferredExchange, "' not found.");
      }

      // --- Step 6: Fallback to SMART or first available ---
      if (auto smart = findByExchange("SMART"); smart != allChains.end()) {
        LOG_DEBUG("[IB] Using SMART option chain");
        return *smart;
      }

      LOG_DEBUG("[IB] Defaulting to first available chain: ", allChains.front().exchange);
      return allChains.front();

    }, "GetOptionChain"); // Label for perf timer
  }

}  // namespace IB::Request

#endif  // QUANTDREAMCPP_OPTIONCHAIN_H

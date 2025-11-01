//
// Created by user on 10/17/25.
//

#ifndef QUANTDREAMCPP_OPTIONCHAIN_H
#define QUANTDREAMCPP_OPTIONCHAIN_H

#include "IBRequestIds.h"
#include "request/contracts/ContractDetails.h"
#include "request/market_data/market_data.h"
#include "wrappers/IBMarketWrapper.h"

/**
 * @file chain.h
 * @brief Option chain retrieval and filtering functionality for Interactive Brokers API
 *
 * This file provides a comprehensive function to retrieve option chain data from IB,
 * including automatic underlying contract resolution, strike filtering based on current
 * price, and intelligent exchange selection. The function handles the complete workflow
 * from contract lookup to filtered chain delivery.
 */

namespace IB::Request {

  /**
   * @brief Synchronously retrieves and filters option chain data for an underlying asset
   *
   * @tparam T Wrapper type that must inherit from IBMarketWrapper
   * @param ib Reference to the IBMarketWrapper instance (or derived class)
   * @param underlying Contract specification for the underlying asset (e.g., stock, index)
   * @param reqId Request identifier for tracking (default: OPTION_CHAIN_ID)
   * @param strikeRangePct Percentage range around current price to filter strikes (default: 0.25 = ±25%)
   * @param preferredExchange Preferred exchange to select (default: "" = auto-select SMART or first)
   * @return ChainInfo object containing filtered strikes, expirations, and exchange metadata
   *
   * This function performs a complete option chain retrieval workflow:
   *
   * **Step 1: Contract Resolution**
   * - Resolves the underlying contract to obtain a valid conId
   * - Ensures the contract is properly identified in the IB system
   * - Returns empty ChainInfo if resolution fails
   *
   * **Step 2: Chain Definition Request**
   * - Requests option chain definitions from all available exchanges
   * - Uses reqSecDefOptParams() with the resolved contract ID
   * - Synchronously waits for all chain data via promise-future mechanism
   *
   * **Step 3: Price Retrieval**
   * - Fetches current last price for the underlying asset
   * - Uses this price as the center point for strike filtering
   * - Returns empty ChainInfo if price retrieval fails
   *
   * **Step 4: Strike Filtering**
   * - Calculates strike range as: [lastPrice × (1 - strikeRangePct), lastPrice × (1 + strikeRangePct)]
   * - Filters all strikes across all exchanges to this range
   * - Logs the calculated range for debugging
   *
   * **Step 5: Exchange Selection**
   * - If preferredExchange is specified and available, selects that exchange
   * - Otherwise, attempts to select "SMART" exchange
   * - Falls back to first available exchange if neither is found
   * - Logs the selection decision for transparency
   *
   * @note The function blocks until all data is retrieved. Performance is automatically
   *       measured and logged via the "GetOptionChain" label.
   *
   * @note All strikes are filtered uniformly across exchanges. The filtering happens
   *       before exchange selection, ensuring consistent data regardless of which
   *       exchange is ultimately chosen.
   *
   * @warning Returns an empty ChainInfo object (with empty strikes/expirations sets)
   *          if any critical step fails: contract resolution, chain retrieval, or
   *          price retrieval.
   *
   * Example usage:
   * @code
   * Contract aapl;
   * aapl.symbol = "AAPL";
   * aapl.secType = "STK";
   * aapl.exchange = "SMART";
   * aapl.currency = "USD";
   *
   * // Get chain with strikes within ±15% of current price, prefer CBOE
   * auto chain = IB::Request::getOptionChain(ib, aapl, IB::ReqId::OPTION_CHAIN_ID, 0.15, "CBOE");
   *
   * std::cout << "Exchange: " << chain.exchange << std::endl;
   * std::cout << "Available expirations: " << chain.expirations.size() << std::endl;
   * std::cout << "Strikes in range: " << chain.strikes.size() << std::endl;
   * @endcode
   */
  template <typename T>
  requires std::is_base_of_v<IBMarketWrapper, T>
  inline IB::Options::ChainInfo getOptionChain(
      T& ib,
      const Contract& underlying,
      int reqId = IB::ReqId::OPTION_CHAIN_ID,
      double strikeRangePct = 0.25,
      const std::string& preferredExchange = "") {
    // Run perf timer
    return IB::Helpers::measure([&]() -> IB::Options::ChainInfo {
      LOG_SECTION("Chain Request");

      // --- Step 1: Resolve the underlying contract (ensure conId is valid) ---
      Contract resolved = IB::Requests::getContractDetails(ib, underlying);
      if (resolved.conId == 0) {
        LOG_ERROR("[IB] getOptionChain: Unable to resolve underlying contract for ", underlying.symbol);
        return {};
      }

      // --- Step 2: Request option chain definitions ---
      auto allChains = IBBaseWrapper::getSync<std::vector<IB::Options::ChainInfo>>(ib, reqId, [&]() {
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

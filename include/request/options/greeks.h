#ifndef QUANTDREAMCPP_GREEKS_H
#define QUANTDREAMCPP_GREEKS_H

#include <cmath>  // for std::isnan, std::isinf

#include "IBRequestIds.h"
#include "contracts/OptionContract.h"
#include "data_structures/greeks_table.h"
#include "wrappers/IBBaseWrapper.h"

/**
 * @file greeks.h
 * @brief Batch retrieval of option Greeks data from Interactive Brokers API
 *
 * This file provides functionality to request and synchronously retrieve option Greeks
 * (delta, gamma, vega, theta, implied volatility) for an entire option chain. It uses
 * intelligent batching and throttling to respect IB's market data subscription limits
 * while efficiently collecting data for multiple strikes and expirations.
 */

namespace IB::Requests {

/**
 * @brief (WORK IN PROGRESS) Synchronously retrieves Greeks for all options in a chain with intelligent batching
 *
 * @param ib Reference to the IBBaseWrapper instance (or derived class)
 * @param underlying Contract specification for the underlying asset
 * @param chain ChainInfo containing strikes, expirations, and exchange details
 * @param right Option right - "C" for calls, "P" for puts (default: "C")
 * @param baseReqId Base request identifier for tracking (default: OPTION_CHAIN_GREEKS_ID)
 * @param batchSize Number of simultaneous market data requests per batch (default: 40)
 * @param delayMsBetweenBatches Milliseconds to wait between batches (default: 1200ms)
 * @return Vector of Greeks objects containing all successfully retrieved option data
 *
 * This function performs a comprehensive batch retrieval of option Greeks data:
 *
 * **Step 1: Promise Setup**
 * - Creates a promise-future pair for synchronous blocking until all data is received
 * - Initializes result storage with thread-safe mutex protection
 * - Calculates total expected responses (expirations × strikes)
 *
 * **Step 2: Callback Registration**
 * - Registers onOptionGreeks callback to capture incoming Greeks data
 * - Validates received data (IB sends DBL_MAX for unavailable values)
 * - Automatically cancels each market data subscription after Greeks are received
 * - Uses atomic counter to track remaining responses
 * - Fulfills promise when all expected responses are received
 *
 * **Step 3: Contract Resolution**
 * - Iterates through all expiration/strike combinations
 * - Creates option contract specifications using makeOption()
 * - Resolves full contract details to populate conId and localSymbol
 * - Skips contracts that fail resolution (logs warning and decrements counter)
 *
 * **Step 4: Batched Market Data Requests**
 * - Submits reqMktData() calls with generic tick "106" for Greeks
 * - Processes requests in batches to stay under IB's subscription limits
 * - Inserts configurable delay between batches to prevent rate limiting
 * - Logs progress every batch for monitoring
 *
 * **Step 5: Synchronous Wait**
 * - Blocks on future.get() until all callbacks complete
 * - Returns collected Greeks data in a vector
 * - Logs summary of received entries
 *
 * @note The function blocks the calling thread until all Greeks data is received.
 *       Total execution time depends on the number of options, batch size, and delay.
 *
 * @note Invalid Greeks entries (where IB returns DBL_MAX for any value) are automatically
 *       filtered out. This typically occurs for illiquid or out-of-range strikes.
 *
 * @note Market data subscriptions are automatically canceled after Greeks are received
 *       to free up subscription slots for subsequent requests.
 *
 * @warning Batch size and delay must be tuned to your IB account's market data limits.
 *          Exceeding limits will result in "Max number of market data requests" errors.
 *          Default values (40 requests, 1200ms delay) are conservative for most accounts.
 *
 * @warning The function assumes the underlying contract has valid symbol, exchange, and
 *          currency fields. Missing or invalid fields may cause contract resolution failures.
 *
 * Example usage:
 * @code
 * // Get chain first
 * auto chain = IB::Request::getOptionChain(ib, underlying);
 *
 * // Retrieve Greeks for all calls with custom batching
 * auto callGreeks = IB::Requests::getGreeksTable(
 *     ib,
 *     underlying,
 *     chain,
 *     "C",           // Calls
 *     IB::ReqId::OPTION_CHAIN_GREEKS_ID,
 *     50,            // 50 requests per batch
 *     1000           // 1 second between batches
 * );
 *
 * // Process results
 * for (const auto& g : callGreeks) {
 *     std::cout << "Strike: " << g.strike
 *               << " Delta: " << g.delta
 *               << " IV: " << g.impliedVol << std::endl;
 * }
 * @endcode
 */
inline std::vector<IB::Options::Greeks> getGreeksTable(
    IBBaseWrapper& ib,
    const Contract& underlying,
    const IB::Options::ChainInfo& chain,
    const std::string& right = "C",
    int baseReqId = IB::ReqId::OPTION_CHAIN_GREEKS_ID,
    size_t batchSize = 40,            // simultaneous requests
    int delayMsBetweenBatches = 1200  // ms delay between batches
) {
  using namespace std::chrono_literals;

  // --- Create a promise to fulfill when all Greeks are received ---
  auto future = ib.createPromise<std::vector<IB::Options::Greeks>>(baseReqId);

  std::mutex mtx;
  std::vector<IB::Options::Greeks> results;
  std::atomic<int> remaining =
      static_cast<int>(chain.expirations.size() * chain.strikes.size());

  LOG_INFO("[IB] Requesting Greeks for ",
           (right == "C" ? "CALLS" : "PUTS"),
           " on ", underlying.symbol,
           " (", chain.expirations.size(), " expirations × ",
           chain.strikes.size(), " strikes, batchSize=", batchSize, ")");

  // --- Set up callback to capture Greeks and cancel subscriptions ---
  ib.onOptionGreeks = [&](TickerId id, const IB::Options::Greeks& g) {
    // Ignore invalid data (IB sends DBL_MAX for unavailable values)
    if (g.impliedVol == DBL_MAX || g.delta == DBL_MAX ||
        g.gamma == DBL_MAX || g.vega == DBL_MAX ||
        g.theta == DBL_MAX || g.optPrice == DBL_MAX)
      return;

    {
      std::lock_guard<std::mutex> lock(mtx);
      results.push_back(g);
      ib.client->cancelMktData(id);  // ✅ stop streaming this option
      LOG_DEBUG("[IB] Received valid Greeks, canceled reqId=", id,
                " (remaining=", remaining - 1, ")");
    }

    if (--remaining == 0) {
      LOG_INFO("[IB] All Greeks received for ", right, ". Fulfilling promise.");
      ib.fulfillPromise(baseReqId, results);
    }
  };

  // --- Request market data for each option in batches ---
  int reqId = baseReqId + 1;
  int count = 0;

  for (const auto& exp : chain.expirations) {
    for (double strike : chain.strikes) {
      Contract opt = IB::Contracts::makeOption(
          underlying.symbol,
          exp,
          strike,
          right,
          chain.exchange.empty() ? "SMART" : chain.exchange,
          underlying.currency.empty() ? "USD" : underlying.currency,
          chain.multiplier.empty() ? "100" : chain.multiplier,
          chain.tradingClass
      );

      // Resolve full contract details (populate conId/localSymbol)
      Contract details = IB::Requests::getContractDetails(ib, opt);
      if (details.conId == 0) {
        LOG_WARN("[IB] Skipping unresolved option ", opt.symbol,
                 " ", exp, " ", strike, right);
        --remaining;
        continue;
      }

      opt.conId = details.conId;
      opt.localSymbol = details.localSymbol;
      opt.tradingClass = details.tradingClass;

      ib.client->reqMktData(reqId++, opt, "106", false, false, nullptr);
      ++count;

      // --- Throttle requests to stay under IB ticker cap ---
      if (count % batchSize == 0) {
        LOG_DEBUG("[IB] Sent ", count, " requests — throttling for ",
                  delayMsBetweenBatches, " ms");
        std::this_thread::sleep_for(
            std::chrono::milliseconds(delayMsBetweenBatches));
      }
    }
  }

  // --- Wait for all Greeks or timeout if necessary ---
  auto result = future.get();
  LOG_INFO("[IB] Completed synchronous Greeks request for ", right,
           " (received ", result.size(), " entries)");

  return result;
}

}  // namespace IB::Requests

#endif  // QUANTDREAMCPP_GREEKS_H

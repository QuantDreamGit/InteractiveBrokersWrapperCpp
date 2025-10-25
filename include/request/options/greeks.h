//
// Created by user on 10/19/25.
//

#ifndef QUANTDREAMCPP_GREEKS_H
#define QUANTDREAMCPP_GREEKS_H

#include "IBRequestIds.h"
#include "contracts/OptionContract.h"
#include "data_structures/greeks_table.h"
#include "wrappers/IBWrapperBase.h"
#include <cmath> // for std::isnan, std::isinf

namespace IB::Requests {



inline std::vector<IB::Options::Greeks> getGreeksTable(
    IBWrapperBase& ib,
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

//
// Created by user on 10/15/25.
//

#ifndef QUANTDREAMCPP_MARKETDATAREQUESTS_H
#define QUANTDREAMCPP_MARKETDATAREQUESTS_H

#include "IBRequestIds.h"
#include "helpers/perf_timer.h"
#include "wrappers/IBBaseWrapper.h"

namespace IB::Requests {

  /**
   * @brief Request a full option/market snapshot (bid/ask + Greeks if available)
   */
  inline MarketData::MarketSnapshot getSnapshot(
      IBBaseWrapper& ib,
      const Contract& contract,
      int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto& snap = ib.snapshotData[reqId];
      snap = {};  // reset clean
      snap.mode = IB::MarketData::PriceType::SNAPSHOT;
      snap.fulfilled = false;
      snap.cancelled = false;

      ib.reqIdToContract[reqId] = contract;

      return IBBaseWrapper::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
        ib.client->reqMktData(reqId, contract, "", false, false, nullptr);
      });
    }, "getSnapshot");
  }

  /**
   * @brief Request quotes only (bid/ask), ignoring Greeks entirely.
   */
  inline MarketData::MarketSnapshot getQuotes(
      IBBaseWrapper& ib,
      const Contract& contract,
      bool streaming = false,
      int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    auto& snap = ib.snapshotData[reqId];
    snap = {};
    snap.mode = IB::MarketData::PriceType::QUOTES_ONLY;
    snap.fulfilled = false;
    snap.cancelled = false;
    snap.streaming = streaming;

    ib.reqIdToContract[reqId] = contract;

    return IBBaseWrapper::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
      ib.client->reqMktData(
          reqId,
          contract,
          "",
          !streaming,                 // snapshot flag: true for one-off, false for live
          false,
          nullptr);
    });
  }

  /**
   * @brief Request Greeks only (no bid/ask fulfillment required)
   */
  inline MarketData::MarketSnapshot getGreeksOnly(
      IBBaseWrapper& ib,
      const Contract& contract,
      int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto& snap = ib.snapshotData[reqId];
      snap = {};  // reset
      snap.mode = IB::MarketData::PriceType::GREEKS_ONLY;
      snap.fulfilled = false;
      snap.cancelled = false;

      ib.reqIdToContract[reqId] = contract;

      return IBBaseWrapper::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
        ib.client->reqMktData(reqId, contract, "", false, false, nullptr);
      });
    }, "getGreeksOnly");
  }

  /**
   * @brief Request last traded price (of underlying or option)
   */
  inline double getLast(
      IBBaseWrapper& ib,
      const Contract& contract,
      int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto& snap = ib.snapshotData[reqId];
      snap = {};
      snap.mode = IB::MarketData::PriceType::LAST;
      snap.fulfilled = false;
      snap.cancelled = false;

      ib.reqIdToContract[reqId] = contract;

      auto result = IBBaseWrapper::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
        ib.client->reqMktData(reqId, contract, "", false, false, nullptr);
      });
      return result.last;
    }, "getLast");
  }

  /**
   * @brief Request only bid price.
   */
  inline double getBid(
      IBBaseWrapper& ib,
      const Contract& contract,
      int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto& snap = ib.snapshotData[reqId];
      snap = {};
      snap.mode = IB::MarketData::PriceType::BID;
      snap.fulfilled = false;
      snap.cancelled = false;

      ib.reqIdToContract[reqId] = contract;

      auto result = IBBaseWrapper::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
        ib.client->reqMktData(reqId, contract, "", false, false, nullptr);
      });
      return result.bid;
    }, "getBid");
  }

  /**
   * @brief Request only ask price.
   */
  inline double getAsk(
      IBBaseWrapper& ib,
      const Contract& contract,
      int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto& snap = ib.snapshotData[reqId];
      snap = {};
      snap.mode = IB::MarketData::PriceType::ASK;
      snap.fulfilled = false;
      snap.cancelled = false;

      ib.reqIdToContract[reqId] = contract;

      auto result = IBBaseWrapper::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
        ib.client->reqMktData(reqId, contract, "", false, false, nullptr);
      });
      return result.ask;
    }, "getAsk");
  }

  /**
   * @brief Compute a midprice (average of bid and ask)
   */
  inline double getMid(
      IBBaseWrapper& ib,
      const Contract& contract,
      int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto snap = IB::Requests::getQuotes(ib, contract, true, reqId);  // uses QUOTES_ONLY mode internally
      ib.client->cancelMktData(reqId);
      if (snap.bid <= 0.0 && snap.ask <= 0.0)
        return 0.0;
      if (snap.bid > 0.0 && snap.ask > 0.0)
        return (snap.bid + snap.ask) / 2.0;
      return (snap.bid > 0.0 ? snap.bid : snap.ask);
    }, "getMid");
  }

}  // namespace IB::Requests

#endif  // QUANTDREAMCPP_MARKETDATAREQUESTS_H

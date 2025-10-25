//
// Created by user on 10/19/25.
//

#ifndef QUANTDREAMCPP_GET_PRICES_H
#define QUANTDREAMCPP_GET_PRICES_H

#include "IBRequestIds.h"
#include "wrappers/IBWrapperBase.h"
#include "helpers/perf_timer.h"

namespace IB::Requests {

  inline MarketData::MarketSnapshot getSnapshot(IBWrapperBase& ib, const Contract& contract,
                                                int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto& snap = ib.snapshotData[reqId];
      snap.mode = IB::MarketData::PriceType::SNAPSHOT;

      return IBWrapperBase::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
          ib.client->reqMktData(reqId, contract, "", false, false, nullptr);
      });
    }, "getSnapshot");
  }

  inline double getLast(IBWrapperBase& ib, const Contract& contract,
                        int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto& snap = ib.snapshotData[reqId];
      snap.mode = IB::MarketData::PriceType::LAST;

      auto result = IBWrapperBase::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
          ib.client->reqMktData(reqId, contract, "", false, false, nullptr);
      });
      return result.last;
    }, "getLast");
  }

  inline double getBid(IBWrapperBase& ib, const Contract& contract,
                       int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto& snap = ib.snapshotData[reqId];
      snap.mode = IB::MarketData::PriceType::BID;

      auto result = IBWrapperBase::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
          ib.client->reqMktData(reqId, contract, "", false, false, nullptr);
      });
      return result.bid;
    }, "getBid");
  }

  inline double getAsk(IBWrapperBase& ib, const Contract& contract,
                       int reqId = IB::ReqId::MARKET_DATA_ID)
  {
    return IB::Helpers::measure([&]() {
      auto& snap = ib.snapshotData[reqId];
      snap.mode = IB::MarketData::PriceType::ASK;

      auto result = IBWrapperBase::getSync<MarketData::MarketSnapshot>(ib, reqId, [&]() {
          ib.client->reqMktData(reqId, contract, "", false, false, nullptr);
      });
      return result.ask;
    }, "getAsk");
  }

}  // namespace IB::Requests

#endif  // QUANTDREAMCPP_GET_PRICES_H

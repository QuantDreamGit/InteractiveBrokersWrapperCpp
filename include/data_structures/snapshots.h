//
// Created by user on 10/23/25.
//

#ifndef QUANTDREAMCPP_SNAPSHOTS_H
#define QUANTDREAMCPP_SNAPSHOTS_H
namespace IB::MarketData {
  enum class PriceType { LAST, BID, ASK, SNAPSHOT };

  struct MarketSnapshot {
    double bid = 0.0;
    double ask = 0.0;
    double last = 0.0;
    double open = 0.0;
    double close = 0.0;
    double high = 0.0;
    double low = 0.0;

    PriceType mode = PriceType::SNAPSHOT;  // default to full snapshot
  };
}

#endif  // QUANTDREAMCPP_SNAPSHOTS_H

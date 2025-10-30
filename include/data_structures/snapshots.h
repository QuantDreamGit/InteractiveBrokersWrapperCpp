#ifndef QUANTDREAMCPP_SNAPSHOTS_H
#define QUANTDREAMCPP_SNAPSHOTS_H

namespace IB::MarketData {

  enum class PriceType {
    LAST,
    BID,
    ASK,
    SNAPSHOT,     // require both quotes + greeks (default)
    QUOTES_ONLY,  // only require bid/ask
    GREEKS_ONLY   // allow fulfilling only when greeks ready
};

  struct MarketSnapshot {
    // --- Quote fields ---
    double bid = 0.0;
    double ask = 0.0;
    double last = 0.0;
    double open = 0.0;
    double close = 0.0;
    double high = 0.0;
    double low = 0.0;

    // --- Option model fields (Greeks) ---
    double impliedVol = 0.0;
    double delta = 0.0;
    double gamma = 0.0;
    double vega = 0.0;
    double theta = 0.0;
    double optPrice = 0.0;
    double undPrice = 0.0;
    bool hasGreeks = false;

    // --- Meta info ---
    PriceType mode = PriceType::SNAPSHOT;
    bool fulfilled = false;
    bool cancelled = false;
    bool streaming = false;  // false -> snapshot (auto cancel), true -> live stream

    // --- Convenience checks ---
    bool hasBidAsk() const noexcept { return bid > 0 && ask > 0; }

    bool hasGreeksData() const noexcept {
      return hasGreeks && impliedVol > 0 && optPrice > 0 && delta != 0.0;
    }

    bool readyForFulfill() const {
      switch (mode) {
        case PriceType::LAST:
          return last > 0.0;

        case PriceType::BID:
          return bid > 0.0;

        case PriceType::ASK:
          return ask > 0.0;

        case PriceType::QUOTES_ONLY:
          // If one side missing, still fulfill after BID or ASK appears
          return (bid > 0.0 || ask > 0.0);

        case PriceType::SNAPSHOT:
          // If greeks requested, require both quotes + greeks
          if (hasGreeks)
            return (bid > 0.0 || ask > 0.0);
          return (bid > 0.0 && ask > 0.0);

        default:
          return false;
      }
    }
  };

} // namespace IB::MarketData

#endif // QUANTDREAMCPP_SNAPSHOTS_H

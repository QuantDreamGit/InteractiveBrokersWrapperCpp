#ifndef QUANTDREAMCPP_IBMARKETWRAPPER_H
#define QUANTDREAMCPP_IBMARKETWRAPPER_H

#include "IBBaseWrapper.h"
#include "helpers/tick_to_string.h"

/**
 * @class IBMarketWrapper
 * @brief Handles market data, tick events, and option computations.
 */
class IBMarketWrapper : public virtual IBBaseWrapper {
public:
  /**
   * @brief Called when IB sends a price tick (bid, ask, last, open, close, etc.)
   */
  void tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attrib) override {
    if (price < 0) return;

    auto it = snapshotData.find(tickerId);
    if (it == snapshotData.end()) return;
    auto& snap = it->second;

    std::string secType;
    if (auto c = reqIdToContract.find(tickerId); c != reqIdToContract.end())
      secType = c->second.secType;

    switch (field) {
      case BID:  snap.bid  = price; break;
      case ASK:  snap.ask  = price; break;
      case LAST:
      case DELAYED_LAST: snap.last = price; break;
      case OPEN:  snap.open  = price; break;
      case CLOSE: snap.close = price; break;
      case HIGH:  snap.high  = price; break;
      case LOW:   snap.low   = price; break;
      default: break;
    }

    LOG_DEBUG("[tickPrice] ID=", tickerId,
              " Field=", IB::Helpers::tickTypeToString(field),
              " Price=", price,
              " SecType=", secType.empty() ? "UNKNOWN" : secType);

    if (!snap.fulfilled && snap.readyForFulfill()) {
      if (snap.last <= 0 && snap.hasBidAsk())
        snap.last = (snap.bid + snap.ask) / 2.0;

      snap.fulfilled = true;
      fulfillPromise(tickerId, snap);

      if (!snap.streaming && !snap.cancelled) {
        client->cancelMktData(tickerId);
        snap.cancelled = true;
      }

      LOG_DEBUG("[IB] [tickPrice] Fulfilled ", (snap.streaming ? "(streaming)" : "(snapshot)"), " reqId=", tickerId);
      if (!snap.streaming) snapshotData.erase(it);
    }
  }

  void tickSnapshotEnd(int reqId) override {
    auto it = snapshotData.find(reqId);
    if (it == snapshotData.end()) return;
    auto& snap = it->second;

    LOG_INFO("[IB] tickSnapshotEnd(", reqId, ")");

    if (snap.fulfilled) {
      LOG_DEBUG("[IB] Already fulfilled reqId=", reqId, " — ignoring tickSnapshotEnd");
      snapshotData.erase(it);
      return;
    }

    if (snap.readyForFulfill()) {
      snap.fulfilled = true;
      fulfillPromise(reqId, snap);
      LOG_DEBUG("[IB] Fulfilled snapshot at end (reqId=", reqId, ")");
    } else {
      LOG_WARN("[IB] tickSnapshotEnd(", reqId, ") without valid data — returning partial snapshot");
      fulfillPromise(reqId, snap);
    }

    if (!snap.streaming && !snap.cancelled) {
      client->cancelMktData(reqId);
      snap.cancelled = true;
    }

    snapshotData.erase(it);
  }

  void tickSize(TickerId tickerId, TickType field, Decimal size) override {
    double val = static_cast<double>(size);
    LOG_DEBUG("[tickSize]   ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(field),
              "  Size=", val);
  }

  void tickString(TickerId tickerId, TickType tickType, const std::string& value) override {
    LOG_DEBUG("[tickString] ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(tickType),
              "  Value=\"", value, "\"");
  }

  void tickGeneric(TickerId tickerId, TickType tickType, double value) override {
    LOG_DEBUG("[tickGeneric] ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(tickType),
              "  Value=", value);
  }

  void tickOptionComputation(TickerId tickerId, TickType tickType, int tickAttrib,
                             double impliedVol, double delta, double optPrice,
                             double pvDividend, double gamma, double vega,
                             double theta, double undPrice) override {
    if (impliedVol == DBL_MAX && delta == DBL_MAX &&
        gamma == DBL_MAX && vega == DBL_MAX &&
        theta == DBL_MAX && optPrice == DBL_MAX)
      return;

    bool partialModel = (delta == DBL_MAX || optPrice == DBL_MAX);
    if (partialModel) {
      LOG_DEBUG("[IB] [tickOptionComputation] Ignoring partial model tick (no delta/optPrice) for reqId=", tickerId);
      return;
    }

    std::string sym = "UNKNOWN", right = "?", expiry = "";
    double strike = 0.0;
    if (auto it = reqIdToContract.find(tickerId); it != reqIdToContract.end()) {
      const Contract& opt = it->second;
      sym = opt.symbol;
      right = opt.right;
      strike = opt.strike;
      expiry = opt.lastTradeDateOrContractMonth;
    }

    LOG_DEBUG("[tickOptionComputation] ID=", tickerId,
              " ", sym, " ", right, " ", strike,
              " IV=", impliedVol, " Δ=", delta,
              " Γ=", gamma, " Θ=", theta, " ν=", vega,
              " OptPrice=", optPrice,
              " UndPrice=", (undPrice == DBL_MAX ? "N/A" : std::to_string(undPrice)));

    auto it = snapshotData.find(tickerId);
    if (it != snapshotData.end()) {
      auto& snap = it->second;
      if (snap.mode == IB::MarketData::PriceType::QUOTES_ONLY)
        return;

      snap.impliedVol = (impliedVol == DBL_MAX ? 0.0 : impliedVol);
      snap.delta = (delta == DBL_MAX ? 0.0 : delta);
      snap.gamma = (gamma == DBL_MAX ? 0.0 : gamma);
      snap.vega = (vega == DBL_MAX ? 0.0 : vega);
      snap.theta = (theta == DBL_MAX ? 0.0 : theta);
      snap.optPrice = (optPrice == DBL_MAX ? 0.0 : optPrice);
      snap.undPrice = (undPrice == DBL_MAX ? 0.0 : undPrice);
      snap.hasGreeks = true;

      if (!snap.fulfilled && snap.readyForFulfill()) {
        snap.fulfilled = true;
        fulfillPromise(tickerId, snap);

        if (!snap.streaming && !snap.cancelled) {
          client->cancelMktData(tickerId);
          snap.cancelled = true;
        }

        if (!snap.streaming) {
          snapshotData.erase(it);
          LOG_DEBUG("[IB] [tickOptionComputation] Fulfilled + cancelled snapshot reqId=", tickerId);
        } else {
          LOG_DEBUG("[IB] [tickOptionComputation] Fulfilled (streaming) reqId=", tickerId);
        }
      }
    }
  }

  void securityDefinitionOptionalParameter(int reqId, const std::string& exchange,
                                           int underlyingConId, const std::string& tradingClass,
                                           const std::string& multiplier,
                                           const std::set<std::string>& expirations,
                                           const std::set<double>& strikes) override {
    auto& chains = optionChains[reqId];
    auto it = std::find_if(chains.begin(), chains.end(),
                           [&](const IB::Options::ChainInfo& c) { return c.exchange == exchange; });

    if (it == chains.end()) {
      IB::Options::ChainInfo chain;
      chain.exchange = exchange;
      chain.tradingClass = tradingClass;
      chain.multiplier = multiplier;
      chain.expirations = expirations;
      chain.strikes = strikes;
      chains.push_back(std::move(chain));
    } else {
      it->expirations.insert(expirations.begin(), expirations.end());
      it->strikes.insert(strikes.begin(), strikes.end());
    }

    LOG_DEBUG("[IB] Received option chain part for exchange ", exchange,
              " (exp=", expirations.size(), ", strikes=", strikes.size(), ")");
  }

  void securityDefinitionOptionalParameterEnd(int reqId) override {
    auto it = optionChains.find(reqId);
    if (it == optionChains.end()) {
      LOG_WARN("[IB] Option chain end received for unknown reqId ", reqId);
      return;
    }

    LOG_INFO("[IB] Option chain data complete for reqId=", reqId,
             " (", it->second.size(), " exchanges)");

    for (const auto& c : it->second)
      LOG_DEBUG("   - ", c.exchange, " (", c.expirations.size(),
                " expirations, ", c.strikes.size(), " strikes)");

    fulfillPromise(reqId, it->second);
    optionChains.erase(it);
  }

  /** Callback to print contract details
 * @param reqId The request ID
 * @param details The contract details
 */
  void contractDetails(int reqId, const ContractDetails& details) override {
    bool fulfilled = false;

    {
      std::lock_guard<std::mutex> lock(promiseMutex);
      auto it = genericPromises.find(reqId);
      if (it != genericPromises.end()) {
        try {
          // Case 1: user requested full ContractDetails
          auto ptr = std::any_cast<std::shared_ptr<std::promise<ContractDetails>>>(it->second);
          ptr->set_value(details);
          genericPromises.erase(it);
          fulfilled = true;
          LOG_DEBUG("[IB] fulfillPromise<ContractDetails> for reqId=", reqId);
        } catch (const std::bad_any_cast&) {
          // Not ContractDetails — will try Contract next
        }
      }
    }

    if (!fulfilled) {
      // Case 2: user requested only Contract
      fulfillPromise(reqId, details.contract);
    }
  }


  void contractDetailsEnd(int reqId) override {
    LOG_DEBUG("[IB] contractDetailsEnd(", reqId, ")");
    // nothing to fulfill here; contractDetails() already did it
  }

};

#endif

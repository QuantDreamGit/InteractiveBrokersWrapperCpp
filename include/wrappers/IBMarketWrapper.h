#ifndef QUANTDREAMCPP_IBMARKETWRAPPER_H
#define QUANTDREAMCPP_IBMARKETWRAPPER_H

#include "IBBaseWrapper.h"
#include "helpers/tick_to_string.h"

/**
 * @file IBMarketWrapper.h
 * @brief Market data handling and option computations for Interactive Brokers API
 *
 * This file provides functionality for receiving and processing market data including
 * price ticks, sizes, option Greeks, and option chain information from IB TWS.
 */

/**
 * @class IBMarketWrapper
 * @brief Handles market data, tick events, and option computations.
 *
 * Extends IBBaseWrapper to provide specialized handling for market data callbacks
 * including real-time prices, option Greeks, and security definitions.
 */
class IBMarketWrapper : public virtual IBBaseWrapper {
public:
  /**
   * @brief Called when IB sends a price tick (bid, ask, last, open, close, etc.)
   *
   * @param tickerId Unique identifier for the market data request
   * @param field Type of price tick (BID, ASK, LAST, etc.)
   * @param price Price value for the tick
   * @param attrib Tick attributes (e.g., can auto-execute)
   *
   * Updates the market snapshot with price data. When all required fields are
   * received based on the request mode, fulfills the associated promise and
   * optionally cancels the market data subscription for snapshot requests.
   */
  void tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attrib) override {
    if (price < 0) return;

    auto it = snapshotData.find(tickerId);
    if (it == snapshotData.end()) return;
    auto& snap = it->second;

    // Optional: detect secType
    std::string secType;
    if (auto c = reqIdToContract.find(tickerId); c != reqIdToContract.end())
      secType = c->second.secType;

    // Update price fields
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

    // --- Unified readiness check ---
    if (!snap.fulfilled && snap.readyForFulfill()) {
      if (snap.last <= 0 && snap.hasBidAsk())
        snap.last = (snap.bid + snap.ask) / 2.0;

      snap.fulfilled = true;
      fulfillPromise(tickerId, snap);

      // auto-cancel only if snapshot mode
      if (!snap.streaming && !snap.cancelled) {
        client->cancelMktData(tickerId);
        snap.cancelled = true;
      }

      LOG_DEBUG("[IB] [tickPrice] Fulfilled "
                ,(snap.streaming ? "(streaming)" : "(snapshot)")
                ," reqId=",tickerId);

      // clean snapshot entry only for one-off snapshots
      if (!snap.streaming)
        snapshotData.erase(it);
    }
  }

  /**
   * @brief Called when a snapshot request is complete
   *
   * @param reqId Request identifier for the completed snapshot
   *
   * Finalizes snapshot data and fulfills associated promises. If data is ready,
   * fulfills immediately; otherwise logs a warning about partial data and fulfills
   * anyway. Cancels market data subscription and removes snapshot entry.
   */
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

  /**
   * @brief Called when IB sends a size tick (bid size, ask size, volume, etc.)
   *
   * @param tickerId Unique identifier for the market data request
   * @param field Type of size tick
   * @param size Size value for the tick
   *
   * Logs size information for debugging purposes. Size data is not currently
   * used to determine snapshot fulfillment.
   */
  void tickSize(TickerId tickerId, TickType field, Decimal size) override {
    double val = static_cast<double>(size);
    LOG_DEBUG("[tickSize]   ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(field),
              "  Size=", val);
  }

  /**
   * @brief Called for string-valued tick updates
   *
   * @param tickerId Unique identifier for the market data request
   * @param tickType Type of string tick (e.g., LAST_TIMESTAMP)
   * @param value String value for the tick
   *
   * Logs string tick data for debugging purposes (e.g., timestamps, exchange names).
   */
  void tickString(TickerId tickerId, TickType tickType, const std::string& value) override {
    LOG_DEBUG("[tickString] ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(tickType),
              "  Value=\"", value, "\"");
  }

  /**
   * @brief Called for generic numerical ticks
   *
   * @param tickerId Unique identifier for the market data request
   * @param tickType Type of generic tick
   * @param value Numerical value for the tick
   *
   * Logs generic tick data for debugging purposes (e.g., mark price, option implied vol).
   */
  void tickGeneric(TickerId tickerId, TickType tickType, double value) override {
    LOG_DEBUG("[tickGeneric] ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(tickType),
              "  Value=", value);
  }

  /**
   * @brief Called when IB sends option model computations (Greeks, IV, etc.)
   *
   * @param tickerId Unique identifier for the option data request
   * @param tickType Type of option computation tick
   * @param tickAttrib Tick attributes
   * @param impliedVol Implied volatility (DBL_MAX if unavailable)
   * @param delta Option delta - rate of change (DBL_MAX if unavailable)
   * @param optPrice Theoretical option price (DBL_MAX if unavailable)
   * @param pvDividend Present value of dividends (DBL_MAX if unavailable)
   * @param gamma Option gamma - rate of delta change (DBL_MAX if unavailable)
   * @param vega Option vega - sensitivity to volatility (DBL_MAX if unavailable)
   * @param theta Option theta - time decay (DBL_MAX if unavailable)
   * @param undPrice Underlying asset price (DBL_MAX if unavailable)
   *
   * Processes option Greeks and updates market snapshots. Ignores completely empty
   * updates and partial model ticks (only IV without delta/optPrice). When all
   * required data according to the request mode is received, fulfills the promise
   * and optionally cancels the subscription for snapshot requests.
   */
  void tickOptionComputation(TickerId tickerId, TickType tickType, int tickAttrib,
                             double impliedVol, double delta, double optPrice,
                             double pvDividend, double gamma, double vega,
                             double theta, double undPrice) override {
    // --- Step 1. Ignore completely empty updates ---
    if (impliedVol == DBL_MAX && delta == DBL_MAX &&
        gamma == DBL_MAX && vega == DBL_MAX &&
        theta == DBL_MAX && optPrice == DBL_MAX)
      return;

    // --- Step 2. Ignore *partial* model ticks (only IV available) ---
    bool partialModel = (delta == DBL_MAX || optPrice == DBL_MAX);
    if (partialModel) {
      LOG_DEBUG("[IB] [tickOptionComputation] Ignoring partial model tick (no delta/optPrice) for reqId=", tickerId);
      return;
    }

    // --- Step 3. Resolve metadata (for logging and mapping) ---
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
              " IV=", impliedVol,
              " Δ=", delta,
              " Γ=", gamma,
              " Θ=", theta,
              " ν=", vega,
              " OptPrice=", optPrice,
              " UndPrice=", (undPrice == DBL_MAX ? "N/A" : std::to_string(undPrice)));

    // --- Step 4. Merge into existing snapshot (if any) ---
    auto it = snapshotData.find(tickerId);
    if (it != snapshotData.end()) {
      auto& snap = it->second;

      // Ignore Greeks entirely if QUOTES_ONLY mode
      if (snap.mode == IB::MarketData::PriceType::QUOTES_ONLY)
        return;

      // Fill Greeks fields
      snap.impliedVol = (impliedVol == DBL_MAX ? 0.0 : impliedVol);
      snap.delta      = (delta      == DBL_MAX ? 0.0 : delta);
      snap.gamma      = (gamma      == DBL_MAX ? 0.0 : gamma);
      snap.vega       = (vega       == DBL_MAX ? 0.0 : vega);
      snap.theta      = (theta      == DBL_MAX ? 0.0 : theta);
      snap.optPrice   = (optPrice   == DBL_MAX ? 0.0 : optPrice);
      snap.undPrice   = (undPrice   == DBL_MAX ? 0.0 : undPrice);
      snap.hasGreeks  = true;

      // Fulfill only when ready according to mode
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

  /**
   * @brief Called for each part of an option chain definition
   *
   * @param reqId Request identifier for the option chain request
   * @param exchange Exchange name (e.g., "SMART", "CBOE")
   * @param underlyingConId Contract ID of the underlying asset
   * @param tradingClass Trading class of the option
   * @param multiplier Contract multiplier (e.g., "100" for equity options)
   * @param expirations Set of available expiration dates (YYYYMMDD format)
   * @param strikes Set of available strike prices
   *
   * Accumulates option chain data across multiple callbacks. If the exchange
   * already exists in the buffer, merges expiration dates and strikes; otherwise
   * creates a new ChainInfo entry. Multiple exchanges may report for the same
   * underlying.
   */
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
      // Merge with existing entry
      it->expirations.insert(expirations.begin(), expirations.end());
      it->strikes.insert(strikes.begin(), strikes.end());
    }

    LOG_DEBUG("[IB] Received option chain part for exchange ", exchange,
              " (exp=", expirations.size(), ", strikes=", strikes.size(), ")");
  }

  /**
   * @brief Called when option chain definition is complete
   *
   * @param reqId Request identifier for the completed option chain request
   *
   * Fulfills the promise with all collected option chain data (vector of ChainInfo
   * objects, one per exchange). Logs summary information about the number of
   * exchanges and their expiration/strike counts, then cleans up the buffer.
   */
  void securityDefinitionOptionalParameterEnd(int reqId) override {
    auto it = optionChains.find(reqId);
    if (it == optionChains.end()) {
      LOG_WARN("[IB] Option chain end received for unknown reqId ", reqId);
      return;
    }

    LOG_INFO("[IB] Option chain data complete for reqId=", reqId,
             " (", it->second.size(), " exchanges)");

    for (const auto& c : it->second) {
      LOG_DEBUG("   - ", c.exchange, " (", c.expirations.size(),
                " expirations, ", c.strikes.size(), " strikes)");
    }

    // Fulfill with all exchanges
    fulfillPromise(reqId, it->second);
    optionChains.erase(it);
  }

  /**
   * @brief Callback invoked when contract details are received
   *
   * @param reqId Request identifier for the contract details request
   * @param details Full contract details from IB
   *
   * Attempts to fulfill promises with either full ContractDetails or just the
   * Contract portion, depending on the type expected by the promise. This allows
   * flexibility in what the caller requests.
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

  /**
   * @brief Callback invoked when all contract details have been received
   *
   * @param reqId Request identifier for the completed contract details request
   *
   * Signals completion of contract details transmission. The actual fulfillment
   * happens in contractDetails() callback, so this is primarily for logging.
   */
  void contractDetailsEnd(int reqId) override {
    LOG_DEBUG("[IB] contractDetailsEnd(", reqId, ")");
    // nothing to fulfill here; contractDetails() already did it
  }
};

#endif

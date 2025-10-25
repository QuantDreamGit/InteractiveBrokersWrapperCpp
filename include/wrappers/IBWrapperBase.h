//
// Created by user on 10/15/25.
//

#ifndef QUANTDREAMCPP_IBWRAPPERBASE_H
#define QUANTDREAMCPP_IBWRAPPERBASE_H

// Standard includes
#include <any>
#include <atomic>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

// IB API includes
#include "EClientSocket.h"
#include "EReader.h"
#include "EReaderOSSignal.h"
#include "EWrapper.h"
#include "EWrapperDefault.h"
#include "Decimal.h"

// Data structures includes
#include "data_structures/greeks_table.h"
#include "data_structures/options.h"
#include "data_structures/snapshots.h"

// Helpers includes
#include "IBRequestIds.h"
#include "data_structures/open_orders.h"
#include "helpers/logger.h"
#include "helpers/tick_to_string.h"

class IBWrapperBase : public EWrapperDefault {
protected:
  // Reader timeout in milliseconds
  // If no data is received within this time, the reader will timeout
  // and will be called anyway
  // This is done to avoid silent deadlocks
  size_t reader_timeout = 100;

  // Flag to control the reader thread
  // When destructor is called, this flag is set to false to stop the reader thread and join it
  std::atomic<bool> running{false};

  // Thread to run the reader
  std::thread reader_thread;
public:
  // Mutex to protect access to the promises map
  std::mutex promiseMutex;

  // Order ID
  std::atomic<int> nextValidOrderId = IB::ReqId::BASE_ORDER_ID;

  // Buffers to collect data parts
  std::unordered_map<int, std::any> genericPromises;                        // Generic promises map
  std::unordered_map<int, IB::MarketData::MarketSnapshot> snapshotData;         // Create a storage for snapshot data
  std::unordered_map<int, std::vector<IB::Options::ChainInfo>> optionChains;// Store multiple chains per reqId
  std::unordered_map<TickerId, Contract> reqIdToContract;

  // Open Orders Management
  std::vector<IB::Orders::OpenOrdersInfo> openOrdersBuffer;
  std::function<void(const IB::Orders::OpenOrdersInfo&)> onOpenOrder;
  std::function<void()> onOpenOrdersComplete;
  std::mutex openOrdersMutex;

  // Callback for option Greek data
  std::function<void(TickerId, const IB::Options::Greeks&)> onOptionGreeks;

  // EReaderOSSignal (OS -> Operating System) act as synchronization object
  // It's job is to wake up reader when new data from socket is received
  EReaderOSSignal signal;
  // Create a unique pointer of the client socket to avoid mistakes
  std::unique_ptr<EClientSocket> client;

  /**
   * Constructor: initializes the EClientSocket with this wrapper and the signal
   */
  IBWrapperBase() : signal(reader_timeout) {
    client = std::make_unique<EClientSocket>(this, &signal);
  }

  /**
   * Destructor: disconnects from TWS if connected and joins the reader thread
   */
  ~IBWrapperBase() override { IBWrapperBase::disconnect(); }

  virtual void disconnect() {
    // Set running to false to stop the reader thread
    running = false;

    // Disconnect from TWS if connected
    if (client && client->isConnected()) {
      client->eDisconnect();
      LOG_INFO("Disconnected from IB TWS");
    }

    // Wake up reader thread if waiting
    signal.issueSignal();

    // Then, join the reader thread if joinable
    if (reader_thread.joinable()) {
      reader_thread.join();
      LOG_DEBUG("Reader thread joined");
    }
  }

  /**
   * This function is called when the connection to TWS is acknowledged
   * It starts the reader thread which will read messages from TWS
   * @note Theoretically, the reader thread can be started after connection is well established,
   *       but for performance reasons, it is started as soon as the connection is acknowledged
   *       to be ready to read messages as soon as they arrive.
   *       In practice, this is not an issue as the connection is established very quickly and time
   *       performance is not improved by a significant amount.
   */
  void connectAck() override {
    // Set running to true to start the reader thread
    running = true;

    // Start the reader thread
    LOG_INFO("Connection acknowledged, starting reader thread.");
    reader_thread = std::thread([this]() {
        EReader reader(client.get(), &signal);
        reader.start();
        while (running && client->isConnected()) {
            signal.waitForSignal();
            reader.processMsgs();
        }
    });
  }

  /** * Connect to TWS
   * @param host The host of the TWS (default: "
   * @param port The port of the TWS (default: 7497)
   * @param clientId The client ID (default: 0)
   * @return true if connected, false otherwise
   */
  bool connect(const char* host = "127.0.0.1",
               int port = 4002,
               int clientId = 0) const {

    // Connect to TWS
    if (!client->eConnect(host, port, clientId)) {
      LOG_ERROR("Failed to connect to IB TWS (host=", host, ", port=", port, ", clientId=", clientId, ")");
      return false;
    }

    LOG_INFO("Connected to IB TWS (host=", host, ", port=", port, ", clientId=", clientId, ")");
    return true;
  }

  /**
   * @brief Retrieve a thread-safe snapshot of all current open orders.
   *
   * Logs a concise summary for each order (symbol, action, type, status, filled, remaining, and limit/market info).
   *
   * @return Vector copy of all open orders stored in the wrapper.
   */
  std::vector<IB::Orders::OpenOrdersInfo> getOpenOrders() {
    std::lock_guard<std::mutex> lock(openOrdersMutex);

    if (openOrdersBuffer.empty()) {
      LOG_INFO("[IB] No open orders currently active.");
      return {};
    }

    LOG_INFO("[IB] Retrieved ", openOrdersBuffer.size(), " open order(s):");

    for (const auto& o : openOrdersBuffer) {
      // Some fields may not be populated by IB (especially before execution)
      std::string priceInfo;
      if (o.order.orderType == "LMT")
        priceInfo = " LmtPrice=" + std::to_string(o.order.lmtPrice);
      else if (o.order.orderType == "STP" || o.order.orderType == "STP LMT")
        priceInfo = " StopPrice=" + std::to_string(o.order.auxPrice);

      LOG_INFO("   #", o.orderId,
               " ", o.contract.symbol,
               " ", o.order.action,
               " ", o.order.orderType,
               " (", o.orderState.status, ")",
               " Qty=", DecimalFunctions::decimalToDouble(o.order.totalQuantity),
               priceInfo);
    }

    return openOrdersBuffer;  // Return a snapshot copy
  }


  // ---------------------------------------------------------------
  //                   Generic Promise Management
  // ---------------------------------------------------------------

  /**
   * Creates a promise for a specific request ID and returns the associated future.
   * @tparam ResultType The type of data that will fulfill the promise
   * @param reqId The request ID
   * @return std::future<ResultType> that can be waited on
   */
  template <typename ResultType>
  std::future<ResultType> createPromise(int reqId) {
    auto promisePtr = std::make_shared<std::promise<ResultType>>();
    auto future = promisePtr->get_future();

    {
      std::lock_guard<std::mutex> lock(promiseMutex);
      genericPromises[reqId] = promisePtr;  // store shared_ptr in std::any
    }

    return future;
  }

  /**
   * Fulfills a generic promise by request ID, setting its value and removing it from the map.
   * @tparam ResultType The type of value expected by the promise
   * @param reqId The request ID
   * @param value The value to set for the promise
   */
  template <typename ResultType>
  void fulfillPromise(int reqId, const ResultType& value) {
    std::lock_guard<std::mutex> lock(promiseMutex);
    auto it = genericPromises.find(reqId);
    if (it == genericPromises.end()) {
      LOG_WARN("No promise for reqId ", reqId);
      return;
    }

    try {
      LOG_DEBUG("fulfillPromise<", typeid(ResultType).name(), "> for reqId=", reqId);
      auto storedPtr = std::any_cast<std::shared_ptr<std::promise<ResultType>>>(it->second);
      storedPtr->set_value(value);
    } catch (const std::bad_any_cast& e) {
      LOG_ERROR("Promise type mismatch for reqId ", reqId,
                " (", e.what(), ", expecting ", typeid(ResultType).name(), ")");
      LOG_ERROR("bad_any_cast while fulfilling reqId=", reqId,
                " expected=", typeid(ResultType).name());
      return;
    }

    genericPromises.erase(it);
  }

  /**
   * Generic synchronous request wrapper
   * This helper creates a promise, sends a request, and waits for its completion.
   * @tparam ResultType The type of the result expected
   * @tparam RequestFunc The callable that sends the IB API request
   * @param ib The IBWrapperBase reference
   * @param reqId The request ID
   * @param sendRequest The callable that triggers the IB API call
   * @return ResultType The fulfilled result
   */
  template <typename ResultType, typename RequestFunc>
  static ResultType getSync(IBWrapperBase& ib, int reqId, RequestFunc sendRequest) {
    auto future = ib.createPromise<ResultType>(reqId);
    sendRequest();  // Trigger the IB API request
    return future.get();  // Wait for and return the result
  }

  // ---------------------------------------------------------------
  //               Callbacks from EWrapperDefault
  // ---------------------------------------------------------------
  /**
   * Callback to print a message when the connection is closed
   */
  void connectionClosed() override {
    LOG_WARN("Connection closed");
  }

  /**
   * Callback to print the next valid order ID
   * @param orderId The next valid order ID
   */
  void nextValidId(OrderId orderId) override {
    int newId = static_cast<int>(orderId);

    // Ensure order IDs are monotonically increasing
    if (newId <= nextValidOrderId) {
      newId = nextValidOrderId + 1;
      LOG_WARN("[IB] nextValidId received outdated ID (", orderId,
               "), incrementing to ", newId);
    }

    nextValidOrderId = newId;
    LOG_INFO("[IB] Next valid order ID set to: ", nextValidOrderId);
  }

  // ============================================================================
  // Order lifecycle event: called whenever order status changes
  // ============================================================================
  void orderStatus(
          OrderId orderId,
          const std::string& status,
          Decimal filled,
          Decimal remaining,
          double avgFillPrice,
          long long permId,
          int parentId,
          double lastFillPrice,
          int clientId,
          const std::string& whyHeld,
          double mktCapPrice
  ) override {
    double filledQty    = DecimalFunctions::decimalToDouble(filled);
    double remainingQty = DecimalFunctions::decimalToDouble(remaining);

    if (status == "Submitted" || status == "Filled" || status == "Cancelled") {
      LOG_INFO("[OrderStatus]  #", orderId,
               " | Status=", status,
               " | Filled=", filledQty,
               " | Remaining=", remainingQty,
               " | AvgPrice=", avgFillPrice);
    } else {
      LOG_DEBUG("[OrderStatus]  #", orderId,
                " | Status=", status,
                " | Filled=", filledQty,
                " | Remaining=", remainingQty,
                " | AvgPrice=", avgFillPrice,
                " | LastFill=", lastFillPrice,
                " | permId=", permId,
                " | clientId=", clientId,
                " | WhyHeld=", whyHeld);
    }
  }

  // ============================================================================
  // Called for each open order when reqOpenOrders() or reqAllOpenOrders() runs
  // ============================================================================
  void openOrder(
          OrderId orderId,
          const Contract& contract,
          const Order& order,
          const OrderState& orderState
  ) override {
    IB::Orders::OpenOrdersInfo info;
    info.orderId = static_cast<int>(orderId);
    info.contract = contract;
    info.order = order;
    info.orderState = orderState;

    {
      std::lock_guard<std::mutex> lock(openOrdersMutex);
      openOrdersBuffer.push_back(info);
    }

    LOG_INFO("[OpenOrder]    #", orderId,
             " | Symbol=", contract.symbol,
             " | Action=", order.action,
             " | Type=", order.orderType,
             " | Qty=", DecimalFunctions::decimalToDouble(static_cast<double>(order.totalQuantity)),
             " | Status=", orderState.status);

    if (onOpenOrder)
      onOpenOrder(info);
  }

  // ============================================================================
  // Called once when all openOrder() callbacks are done
  // ============================================================================
  void openOrderEnd() override {
    size_t count = 0;
    {
      std::lock_guard<std::mutex> lock(openOrdersMutex);
      count = openOrdersBuffer.size();
    }

    if (count > 0)
      LOG_INFO("[OpenOrders]   Complete (", count, " total).");
    else
      LOG_INFO("[OpenOrders]   No open orders reported.");

    if (onOpenOrdersComplete)
      onOpenOrdersComplete();

    {
      std::lock_guard<std::mutex> lock(openOrdersMutex);
      openOrdersBuffer.clear();
    }
  }

  /**
   * Helper function to make sure to create a new order with a different orderID
   */
  int nextOrderId() {
    return nextValidOrderId++;
  }

  /**
   * Callback to print the current time
   * @param reqId The request ID
   * @param marketDataType The market data type (1 = real-time, 2 = frozen, 3 = delayed, 4 = delayed frozen)
   */
  void marketDataType(TickerId reqId, int marketDataType) override {
    std::string type;
    switch (marketDataType) {
      case 1: type = "Real-time"; break;
      case 2: type = "Frozen"; break;
      case 3: type = "Delayed"; break;
      case 4: type = "Delayed Frozen"; break;
      default: type = "Unknown"; break;
    }
    LOG_DEBUG("Market data type for request ", reqId, ": ", type);
  }

  /**
   * @brief Called for generic numerical ticks (e.g. mark price, option implied vol, etc.)
   */
  void tickGeneric(TickerId tickerId, TickType tickType, double value) override {
    LOG_DEBUG("[tickGeneric] ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(tickType),
              "  Value=", value);
  }

  /**
   * @brief Called when IB sends a price tick (bid, ask, last, open, close, etc.)
   */
  void tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attrib) override {
    if (price <= 0) return;
    auto& snap = snapshotData[tickerId];

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

    // 🧩 Old debug logging preserved
    LOG_DEBUG("[tickPrice] ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(field),
              "  Price=", price);

    // ✅ Decide when we have enough data to fulfill
    bool ready = false;
    if (snap.mode == IB::MarketData::PriceType::LAST && snap.last > 0)
      ready = true;
    else if (snap.mode == IB::MarketData::PriceType::BID && snap.bid > 0)
      ready = true;
    else if (snap.mode == IB::MarketData::PriceType::ASK && snap.ask > 0)
      ready = true;
    else if (snap.mode == IB::MarketData::PriceType::SNAPSHOT &&
             snap.bid > 0 && snap.ask > 0 && snap.last > 0)
      ready = true;

    // ✅ Fulfill and cancel once ready
    if (ready) {
      fulfillPromise(tickerId, snap);
      client->cancelMktData(tickerId);
      snapshotData.erase(tickerId);

      LOG_DEBUG("[IB] Auto-canceled market data stream after fulfilling mode=",
               IB::Helpers::tickTypeToString(static_cast<TickType>(field)));
    }
  }

  void tickSnapshotEnd(int reqId) override {
    auto it = snapshotData.find(reqId);
    if (it == snapshotData.end()) return;

    LOG_INFO("[IB] tickSnapshotEnd(", reqId, ")");

    fulfillPromise(reqId, it->second);
    snapshotData.erase(it);
  }

  /**
   * @brief Called when IB sends a size tick (bid size, ask size, last size, etc.)
   */
  void tickSize(TickerId tickerId, TickType field, Decimal size) override {
    double val = static_cast<double>(size);
    LOG_DEBUG("[tickSize]   ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(field),
              "  Size=", val);
  }

  /**
   * @brief Called for string-valued tick updates (timestamps, exchange names, etc.)
   */
  void tickString(TickerId tickerId, TickType tickType, const std::string& value) override {
    LOG_DEBUG("[tickString] ID=", tickerId,
              "  Field=", IB::Helpers::tickTypeToString(tickType),
              "  Value=\"", value, "\"");
  }

  /**
   * Callback to print error messages
   * @param id The ID of the request that caused the error (or -1 if not applicable)
   * @param time The time the error occurred
   * @param code The error code
   * @param msg The error message
   */
  void error(int id, time_t time, int code, const std::string& msg, const std::string&) override {
    LOG_ERROR("Error [", code, "] ", msg);
  }

  /** Callback to print contract details
   * @param reqId The request ID
   * @param details The contract details
   */
  void contractDetails(int reqId, const ContractDetails& details) override {
    Contract info = details.contract;

    // Try fulfilling the generic promise
    fulfillPromise(reqId, std::move(info));
  }

  void securityDefinitionOptionalParameter(
      int reqId,
      const std::string& exchange,
      int underlyingConId,
      const std::string& tradingClass,
      const std::string& multiplier,
      const std::set<std::string>& expirations,
      const std::set<double>& strikes) override
  {
    auto& chains = optionChains[reqId];

    // Check if this exchange already exists in our vector
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
   * @brief Called when IB sends option model computation updates (Greeks, implied vol, etc.)
   */
void tickOptionComputation(
    TickerId tickerId,
    TickType tickType,
    int tickAttrib,
    double impliedVol,
    double delta,
    double optPrice,
    double pvDividend,
    double gamma,
    double vega,
    double theta,
    double undPrice
) override
{
    // --- Ignore incomplete values (IB sends DBL_MAX for missing data) ---
    if (impliedVol == DBL_MAX || delta == DBL_MAX || gamma == DBL_MAX ||
        vega == DBL_MAX || theta == DBL_MAX || optPrice == DBL_MAX)
        return;

    LOG_DEBUG("[tickOptionComputation] ID=", tickerId,
              " Field=", IB::Helpers::tickTypeToString(tickType),
              " IV=", impliedVol,
              " Δ=", delta,
              " Γ=", gamma,
              " Θ=", theta,
              " ν=", vega,
              " OptPrice=", optPrice,
              " UndPrice=", undPrice);

    // --- Retrieve associated contract info for this reqId (if stored earlier) ---
    auto it = reqIdToContract.find(tickerId);
    if (it == reqIdToContract.end()) {
        LOG_WARN("[IB] Missing contract metadata for reqId=", tickerId);
        client->cancelMktData(tickerId);
        return;
    }

    const Contract& opt = it->second;

    // --- Build custom Greeks struct with all metadata ---
    IB::Options::Greeks g;
    g.symbol        = opt.symbol;
    g.right         = opt.right;
    g.strike        = opt.strike;
    g.expiry        = opt.lastTradeDateOrContractMonth;
    g.exchange      = opt.exchange;
    g.tradingClass  = opt.tradingClass;
    g.impliedVol    = impliedVol;
    g.delta         = delta;
    g.gamma         = gamma;
    g.vega          = vega;
    g.theta         = theta;
    g.optPrice      = optPrice;
    g.undPrice      = undPrice;

    // --- Forward the completed Greeks to your handler ---
    if (onOptionGreeks)
        onOptionGreeks(tickerId, g);

    // --- Cancel subscription to free ticker slot ---
    client->cancelMktData(tickerId);
    LOG_DEBUG("[IB] Canceled market data subscription for reqId=", tickerId);
}



};



#endif  // QUANTDREAMCPP_IBWRAPPERBASE_H

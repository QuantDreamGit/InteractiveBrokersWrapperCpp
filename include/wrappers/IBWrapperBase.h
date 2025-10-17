//
// Created by user on 10/15/25.
//

#ifndef QUANTDREAMCPP_IBWRAPPERBASE_H
#define QUANTDREAMCPP_IBWRAPPERBASE_H

#include <atomic>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

#include "data_structures/options.h"

#include "EClientSocket.h"
#include "EReader.h"
#include "EReaderOSSignal.h"
#include "EWrapper.h"
#include "EWrapperDefault.h"

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
  // Create Promises map to handle asynchronous requests
  std::unordered_map<int, std::promise<Contract>> contractDetailsPromises;  // ContractDetails
  std::unordered_map<int, std::promise<IB::Options::ChainInfo>> optionChainPromises;

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
      std::cout << "Disconnected from IB TWS" << std::endl;
    }

    // Wake up reader thread if waiting
    signal.issueSignal();
    // Then, join the reader thread if joinable
    if (reader_thread.joinable()) {
      reader_thread.join();
      std::cout << "Reader thread joined" << std::endl;
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
    std::cout << "Connection acknowledged, starting reader thread." << std::endl;
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
  bool connect(char const* host = "127.0.0.1",
               int const port = 7497,
               int const clientId = 0) const {

    // Connect to TWS
    if (!client->eConnect(host, port, clientId)) {
      std::cerr << "Failed to connect to IB TWS" << std::endl;
      return false;
    }
    std::cout << "Connected to IB TWS" << std::endl;
    return true;
  }

  // ---------------------------------------------------------------
  //               Callbacks from EWrapperDefault
  // ---------------------------------------------------------------
  /**
   * Callback to print a message when the connection is closed
   */
  void connectionClosed() override {
    std::cout << "Connection closed" << std::endl;
  }

  /**
   * Callback to print the next valid order ID
   * @param orderId The next valid order ID
   */
  void nextValidId(OrderId orderId) override {
    std::cout << "Next valid order ID: " << orderId << std::endl;
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
    std::cout << "Market data type for request " << reqId << ": " << type << std::endl;
  }

  /**
   * Callback to print tick price updates
   * @param tickerId The ticker ID
   * @param field The tick type (e.g., BID, ASK, LAST)
   * @param price The price
   * @param TickAttrib Additional attributes (not used here)
   */
  void tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib&) override {
    if (price > 0)
      std::cout << "Tick price [" << tickerId << "] field " << field << " = " << price << std::endl;
  }

  /**
   * Callback to print tick size updates
   * @param tickerId The ticker ID
   * @param field The tick type (e.g., BID_SIZE, ASK_SIZE, LAST_SIZE)
   * @param size The size
   */
  void tickSize(TickerId tickerId, TickType field, Decimal size) override {
    std::cout << "Tick size [" << tickerId << "] field " << field
              << " = " << static_cast<double>(size) << std::endl;
  }

  /**
   * Callback to print error messages
   * @param id The ID of the request that caused the error (or -1 if not applicable)
   * @param time The time the error occurred
   * @param code The error code
   * @param msg The error message
   */
  void error(int id, time_t time, int code, const std::string& msg, const std::string&) override {
    std::cerr << "Error [" << code << "] " << msg << std::endl;
  }

  /** Callback to print contract details
   * @param reqId The request ID
   * @param details The contract details
   */
  void contractDetails(int reqId, const ContractDetails& details) override {
    Contract info = details.contract;
    // Fulfill the promise for contract details if exists
    // Lock the mutex to protect the map access
    std::lock_guard<std::mutex> lock(promiseMutex);
    // Find the promise for the given request ID
    auto it = contractDetailsPromises.find(reqId);
    // If found, set the value and erase the promise from the map
    if (it != contractDetailsPromises.end()) {
      // Set the value of the promise
      it->second.set_value(std::move(info));
      contractDetailsPromises.erase(it);
    }
  }

  void securityDefinitionOptionalParameter(
      int reqId,
      const std::string& exchange,
      int underlyingConId,
      const std::string& tradingClass,
      const std::string& multiplier,
      const std::set<std::string>& expirations,
      const std::set<double>& strikes) override {

    // Initialize ChainInfo struct
    IB::Options::ChainInfo chain {
      exchange, tradingClass, multiplier, expirations, strikes
    };

    // Fulfill the promise for option chain if exists
    // Lock the mutex to protect the map access
    std::lock_guard<std::mutex> lock(promiseMutex);
    auto it = optionChainPromises.find(reqId);
    // If found, set the value and erase the promise from the map
    if (it != optionChainPromises.end()) {
      it->second.set_value(std::move(chain));
      optionChainPromises.erase(it);
    }
  }
};


#endif  // QUANTDREAMCPP_IBWRAPPERBASE_H

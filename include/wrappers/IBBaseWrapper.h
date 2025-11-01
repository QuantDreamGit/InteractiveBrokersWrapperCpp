#ifndef QUANTDREAMCPP_IBBASEWRAPPER_H
#define QUANTDREAMCPP_IBBASEWRAPPER_H

#include <any>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "EClientSocket.h"
#include "EReader.h"
#include "EReaderOSSignal.h"
#include "EWrapperDefault.h"
#include "helpers/logger.h"
#include "data_structures/snapshots.h"
#include "data_structures/options.h"
#include "data_structures/positions.h"
#include "IBRequestIds.h"

/**
 * @file IBBaseWrapper.h
 * @brief Core wrapper for Interactive Brokers TWS API communication
 *
 * This file provides the base implementation for interacting with the Interactive Brokers
 * Trader Workstation (TWS) API. It handles connection management, threading for message
 * processing, and promise-based asynchronous request handling.
 */

/**
 * @class IBBaseWrapper
 * @brief Core communication and synchronization layer for the IB API.
 *
 * Extends EWrapperDefault to provide a thread-safe, promise-based interface for
 * communicating with Interactive Brokers TWS. Manages connection lifecycle,
 * message processing, and data synchronization.
 */
class IBBaseWrapper : public virtual EWrapperDefault {
protected:
    size_t reader_timeout = 100; ///< Timeout for reader signal in milliseconds
    std::atomic<bool> running{false}; ///< Flag indicating if reader thread is active
    std::thread reader_thread; ///< Thread for processing incoming messages

public:
    bool initializing = true; ///< Flag indicating initialization state
    std::mutex promiseMutex; ///< Mutex for thread-safe promise access
    std::atomic<int> nextValidOrderId = IB::ReqId::BASE_ORDER_ID; ///< Next available order ID

    std::unordered_map<int, std::any> genericPromises; ///< Map of request IDs to promises
    std::unordered_map<int, IB::MarketData::MarketSnapshot> snapshotData; ///< Market data snapshots by request ID
    std::unordered_map<TickerId, Contract> reqIdToContract; ///< Contract lookup by ticker ID
    std::unordered_map<int, std::vector<IB::Options::ChainInfo>> optionChains; ///< Option chain data by request ID
    std::vector<IB::Accounts::PositionInfo> positionBuffer; ///< Buffer for position information

    EReaderOSSignal signal; ///< OS signal for reader synchronization
    std::unique_ptr<EClientSocket> client; ///< IB API client socket

    /**
     * @brief Default constructor
     *
     * Initializes the wrapper with default reader timeout and creates the client socket.
     */
    IBBaseWrapper() : signal(reader_timeout) {
        client = std::make_unique<EClientSocket>(this, &signal);
    }

    /**
     * @brief Virtual destructor
     *
     * Ensures proper cleanup by disconnecting before destruction.
     */
    virtual ~IBBaseWrapper() override { disconnect(); }

    // ------------------------------------------------------------------
    // Connection and threading
    // ------------------------------------------------------------------

    /**
     * @brief Establishes connection to TWS or IB Gateway
     *
     * @param host Server hostname or IP address (default: "127.0.0.1")
     * @param port Server port number (default: 4002 for IB Gateway)
     * @param clientId Unique client identifier (default: 0)
     * @return true if connection successful, false otherwise
     *
     * Starts the reader thread for processing incoming messages.
     */
    virtual bool connect(const char* host = "127.0.0.1", int port = 4002, int clientId = 0) {
        if (client->isConnected()) {
            LOG_INFO("[IB] [Connection] Already connected.");
            return true;
        }

        LOG_INFO("[IB] [Connection] Connecting to ", host, ":", port, " (clientId=", clientId, ")");
        if (!client->eConnect(host, port, clientId)) {
            LOG_ERROR("[IB] Failed to connect to TWS");
            return false;
        }

        running = true;
        reader_thread = std::thread([this]() {
            EReader reader(client.get(), &signal);
            reader.start();
            signal.issueSignal();
            while (running && client->isConnected()) {
                signal.waitForSignal();
                reader.processMsgs();
            }
            LOG_DEBUG("[IB] Reader thread stopped");
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        return true;
    }

    /**
     * @brief Disconnects from TWS and stops the reader thread
     *
     * Gracefully shuts down the connection and joins the reader thread.
     */
    virtual void disconnect() {
        running = false;
        if (client && client->isConnected()) {
            client->eDisconnect();
            LOG_INFO("Disconnected from TWS");
        }
        signal.issueSignal();
        if (reader_thread.joinable()) reader_thread.join();
    }

    /**
     * @brief Callback invoked when connection is acknowledged
     *
     * Override from EWrapperDefault, logs connection acknowledgment.
     */
    void connectAck() override { LOG_INFO("[IB] Connection ACK"); }

    // ------------------------------------------------------------------
    // Promise Management
    // ------------------------------------------------------------------

    /**
     * @brief Creates a promise for asynchronous request handling
     *
     * @tparam ResultType Type of the expected result
     * @param reqId Unique request identifier
     * @return Future object for retrieving the result
     *
     * Thread-safe creation of promises stored in genericPromises map.
     */
    template<typename ResultType>
    std::future<ResultType> createPromise(int reqId) {
        auto p = std::make_shared<std::promise<ResultType>>();
        auto f = p->get_future();
        std::lock_guard<std::mutex> lock(promiseMutex);
        genericPromises[reqId] = p;
        return f;
    }

    /**
     * @brief Fulfills a promise with a result value
     *
     * @tparam ResultType Type of the result value
     * @param reqId Request identifier of the promise to fulfill
     * @param value Result value to set
     *
     * Thread-safe promise fulfillment with type checking and cleanup.
     */
    template<typename ResultType>
    void fulfillPromise(int reqId, const ResultType& value) {
        std::lock_guard<std::mutex> lock(promiseMutex);
        auto it = genericPromises.find(reqId);
        if (it == genericPromises.end()) return;
        try {
            auto p = std::any_cast<std::shared_ptr<std::promise<ResultType>>>(it->second);
            p->set_value(value);
        } catch (...) {
            LOG_ERROR("[Promise] Type mismatch for reqId=", reqId);
        }
        genericPromises.erase(it);
    }

    /**
     * @brief Synchronously executes a request and waits for the result
     *
     * @tparam ResultType Type of the expected result
     * @tparam RequestFunc Type of the request function
     * @param ib Reference to the IBBaseWrapper instance
     * @param reqId Unique request identifier
     * @param sendRequest Function to send the request
     * @return Result of the request or default-constructed value on error
     *
     * Helper method that creates a promise, sends the request, and waits for completion.
     */
    template<typename ResultType, typename RequestFunc>
    static ResultType getSync(IBBaseWrapper& ib, int reqId, RequestFunc sendRequest) {
        auto fut = ib.createPromise<ResultType>(reqId);
        sendRequest();
        ResultType result{};
        try { result = fut.get(); }
        catch (const std::exception& e) { LOG_ERROR("[getSync] ", e.what()); }
        return result;
    }

    // ------------------------------------------------------------------
    // Misc Callbacks
    // ------------------------------------------------------------------

    /**
     * @brief Callback invoked when connection is closed
     *
     * Override from EWrapperDefault, logs connection closure warning.
     */
    void connectionClosed() override { LOG_WARN("Connection closed"); }

    /**
     * @brief Callback invoked when TWS provides the next valid order ID
     *
     * @param orderId Next valid order ID from TWS
     *
     * Updates the internal order ID counter for new orders.
     */
    void nextValidId(OrderId orderId) override {
        nextValidOrderId = static_cast<int>(orderId);
        LOG_INFO("[IB] NextValidOrderId=", nextValidOrderId);
    }

    /**
     * @brief Gets the next available order ID
     *
     * @return Next order ID (atomically incremented)
     *
     * Thread-safe method to obtain unique order IDs for new orders.
     */
    int nextOrderId() { return nextValidOrderId++; }
};

#endif

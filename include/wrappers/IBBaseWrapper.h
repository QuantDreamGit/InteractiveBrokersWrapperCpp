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
 * @class IBBaseWrapper
 * @brief Core communication and synchronization layer for the IB API.
 */
class IBBaseWrapper : public virtual EWrapperDefault {
protected:
    size_t reader_timeout = 100;
    std::atomic<bool> running{false};
    std::thread reader_thread;

public:
    bool initializing = true;
    std::mutex promiseMutex;
    std::atomic<int> nextValidOrderId = IB::ReqId::BASE_ORDER_ID;

    std::unordered_map<int, std::any> genericPromises;
    std::unordered_map<int, IB::MarketData::MarketSnapshot> snapshotData;
    std::unordered_map<TickerId, Contract> reqIdToContract;
    std::unordered_map<int, std::vector<IB::Options::ChainInfo>> optionChains;
    std::vector<IB::Accounts::PositionInfo> positionBuffer;

    EReaderOSSignal signal;
    std::unique_ptr<EClientSocket> client;

    IBBaseWrapper() : signal(reader_timeout) {
        client = std::make_unique<EClientSocket>(this, &signal);
    }

    virtual ~IBBaseWrapper() override { disconnect(); }

    // ------------------------------------------------------------------
    // Connection and threading
    // ------------------------------------------------------------------

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

    virtual void disconnect() {
        running = false;
        if (client && client->isConnected()) {
            client->eDisconnect();
            LOG_INFO("Disconnected from TWS");
        }
        signal.issueSignal();
        if (reader_thread.joinable()) reader_thread.join();
    }

    void connectAck() override { LOG_INFO("[IB] Connection ACK"); }

    // ------------------------------------------------------------------
    // Promise Management
    // ------------------------------------------------------------------
    template<typename ResultType>
    std::future<ResultType> createPromise(int reqId) {
        auto p = std::make_shared<std::promise<ResultType>>();
        auto f = p->get_future();
        std::lock_guard<std::mutex> lock(promiseMutex);
        genericPromises[reqId] = p;
        return f;
    }

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
    void connectionClosed() override { LOG_WARN("Connection closed"); }

    void nextValidId(OrderId orderId) override {
        nextValidOrderId = static_cast<int>(orderId);
        LOG_INFO("[IB] NextValidOrderId=", nextValidOrderId);
    }

    int nextOrderId() { return nextValidOrderId++; }
};

#endif

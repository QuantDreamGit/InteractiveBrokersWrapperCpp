//
// Created by user on 10/29/25.
//

#ifndef QUANTDREAMCPP_IBWRAPPERMONITOR_H
#define QUANTDREAMCPP_IBWRAPPERMONITOR_H
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "EClientSocket.h"
#include "EReader.h"
#include "EReaderOSSignal.h"
#include "EWrapperDefault.h"
#include "data_structures/open_orders.h"
#include "helpers/logger.h"

class IBWrapperMonitor : public EWrapperDefault {
public:
    std::unique_ptr<EClientSocket> client;
    EReaderOSSignal signal;
    std::atomic<bool> running{false};
    std::thread reader_thread;
    std::atomic<int> nextValidOrderId{0};

    std::mutex ordersMutex;
    std::unordered_map<int, std::string> knownOrderStatuses;  // cache: orderId → status

    IBWrapperMonitor() : signal(100) {
        client = std::make_unique<EClientSocket>(this, &signal);
    }

    ~IBWrapperMonitor() override { disconnect(); }

    bool connect(const char* host = "127.0.0.1", int port = 4002, int clientId = 2) {
        if (!client->eConnect(host, port, clientId)) {
            LOG_ERROR("[Monitor] Failed to connect to IB Gateway/TWS.");
            return false;
        }
        LOG_INFO("[Monitor] Connected to IB Gateway (", host, ":", port, ", clientId=", clientId, ")");
        return true;
    }

    void connectAck() override {
        running = true;
        LOG_INFO("[Monitor] Connection acknowledged, starting reader thread...");
        reader_thread = std::thread([this]() {
            EReader reader(client.get(), &signal);
            reader.start();
            while (running && client->isConnected()) {
                signal.waitForSignal();
                reader.processMsgs();
            }
        });
    }

    void disconnect() {
        running = false;
        if (client && client->isConnected()) {
            client->eDisconnect();
            LOG_INFO("[Monitor] Disconnected from IB Gateway.");
        }
        signal.issueSignal();
        if (reader_thread.joinable())
            reader_thread.join();
    }

    void nextValidId(OrderId orderId) override {
        nextValidOrderId = static_cast<int>(orderId);
        LOG_INFO("[Monitor] Next valid order ID received: ", nextValidOrderId);
    }

    void openOrder(OrderId orderId, const Contract& contract, const Order& order, const OrderState& orderState) override {
        std::lock_guard<std::mutex> lock(ordersMutex);
        std::string key = orderState.status;

        // Only print if new or status changed
        auto it = knownOrderStatuses.find(orderId);
        if (it == knownOrderStatuses.end() || it->second != key) {
            knownOrderStatuses[orderId] = key;

            LOG_INFO("[Monitor] OpenOrder #", orderId,
                     " | Symbol=", contract.symbol,
                     " | Action=", order.action,
                     " | Type=", order.orderType,
                     " | Qty=", DecimalFunctions::decimalToDouble(order.totalQuantity),
                     " | Status=", orderState.status);
        }
    }

    void orderStatus(OrderId orderId, const std::string& status,
                     Decimal filled, Decimal remaining,
                     double avgFillPrice, long long, int, double, int,
                     const std::string&, double) override {
        std::lock_guard<std::mutex> lock(ordersMutex);

        auto it = knownOrderStatuses.find(orderId);
        if (it == knownOrderStatuses.end() || it->second != status) {
            knownOrderStatuses[orderId] = status;

            LOG_INFO("[Monitor] OrderStatus #", orderId,
                     " | Status=", status,
                     " | Filled=", DecimalFunctions::decimalToDouble(filled),
                     " | Remaining=", DecimalFunctions::decimalToDouble(remaining),
                     " | AvgPrice=", avgFillPrice);
        }
    }

    void openOrderEnd() override {
        std::lock_guard<std::mutex> lock(ordersMutex);
        LOG_INFO("[Monitor] OpenOrdersEnd (", knownOrderStatuses.size(), " tracked).");
    }

    void error(int, time_t, int code, const std::string& msg, const std::string&) override {
        LOG_ERROR("[Monitor] Error [", code, "] ", msg);
    }
};
#endif  // QUANTDREAMCPP_IBWRAPPERMONITOR_H

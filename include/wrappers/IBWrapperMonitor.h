//
// Created by user on 10/29/25.
//

#ifndef QUANTDREAMCPP_IBWRAPPERMONITOR_H
#define QUANTDREAMCPP_IBWRAPPERMONITOR_H

#include <atomic>
#include <chrono>
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
    std::thread polling_thread;
    std::atomic<int> nextValidOrderId{0};

    std::mutex ordersMutex;
    std::unordered_map<int, std::string> knownOrderStatuses;  // orderId → status

    IBWrapperMonitor() : signal(100) {
        client = std::make_unique<EClientSocket>(this, &signal);
    }

    ~IBWrapperMonitor() override { disconnect(); }

    // ───────────────────────────────────────────────
    // Connection
    // ───────────────────────────────────────────────
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

        // Start active polling thread
        polling_thread = std::thread([this]() {
            LOG_INFO("[Monitor] Starting periodic open order polling...");
            while (running && client->isConnected()) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                if (client->isConnected()) {
                    client->reqAllOpenOrders();
                }
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
        if (reader_thread.joinable()) reader_thread.join();
        if (polling_thread.joinable()) polling_thread.join();
    }

    // ───────────────────────────────────────────────
    // Core Callbacks
    // ───────────────────────────────────────────────
    void nextValidId(OrderId orderId) override {
        nextValidOrderId = static_cast<int>(orderId);
        LOG_INFO("[Monitor] Next valid order ID received: ", nextValidOrderId);
    }

    void openOrder(OrderId orderId, const Contract& contract, const Order& order, const OrderState& orderState) override {
        std::lock_guard<std::mutex> lock(ordersMutex);
        std::string newStatus = orderState.status;
        std::string oldStatus = knownOrderStatuses[orderId];

        if (newStatus != oldStatus) {
            knownOrderStatuses[orderId] = newStatus;
            LOG_INFO("[Monitor] OpenOrder #", orderId,
                     " | Symbol=", contract.symbol,
                     " | Action=", order.action,
                     " | Type=", order.orderType,
                     " | Qty=", DecimalFunctions::decimalToDouble(order.totalQuantity),
                     " | Status=", newStatus);
        }
    }

    void orderStatus(OrderId orderId, const std::string& status,
                     Decimal filled, Decimal remaining,
                     double avgFillPrice, long long, int, double, int,
                     const std::string&, double) override {
        std::lock_guard<std::mutex> lock(ordersMutex);
        std::string oldStatus = knownOrderStatuses[orderId];
        if (oldStatus != status) {
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

        // Identify missing (cancelled) orders
        static std::unordered_map<int, std::string> previous;
        for (auto it = previous.begin(); it != previous.end();) {
            if (knownOrderStatuses.find(it->first) == knownOrderStatuses.end()) {
                LOG_WARN("[Monitor] Order #", it->first, " appears cancelled or closed.");
                it = previous.erase(it);
            } else {
                ++it;
            }
        }

        previous = knownOrderStatuses;  // snapshot current set
    }

    // ───────────────────────────────────────────────
    // Error handling
    // ───────────────────────────────────────────────
    void error(int id, time_t, int code, const std::string& msg, const std::string&) override {
        if (code == 300 && msg.find("Can't find EId") != std::string::npos)
            return;
        if (code == 2104 || code == 2107 || code == 2158 || code == 2119)
            return;
        LOG_ERROR("[Monitor] Error [", code, "] ", msg);
    }
};

#endif  // QUANTDREAMCPP_IBWRAPPERMONITOR_H

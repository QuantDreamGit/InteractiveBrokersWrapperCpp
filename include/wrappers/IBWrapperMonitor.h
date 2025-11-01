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

/**
 * @file IBWrapperMonitor.h
 * @brief Dedicated monitor for tracking order status changes in real-time
 *
 * This file provides a lightweight wrapper specifically designed for monitoring
 * order status changes without interfering with main trading operations. It uses
 * a separate connection to IB Gateway/TWS with periodic polling to detect status
 * changes, cancellations, and completions in real-time.
 */

/**
 * @class IBWrapperMonitor
 * @brief Standalone monitor for tracking order lifecycle and status changes.
 *
 * A separate connection to IB Gateway/TWS dedicated exclusively to monitoring order status.
 * Uses periodic polling (every 3 seconds) and state tracking to detect order status changes,
 * cancellations, and completions without blocking main trading operations. This allows
 * for reactive processing of order events independently of the primary trading connection.
 */
class IBWrapperMonitor : public EWrapperDefault {
public:
    std::unique_ptr<EClientSocket> client; ///< IB API client socket for monitor connection
    EReaderOSSignal signal; ///< OS signal for reader synchronization (100ms timeout)
    std::atomic<bool> running{false}; ///< Flag indicating monitor is active
    std::thread reader_thread; ///< Thread for processing incoming IB messages
    std::thread polling_thread; ///< Thread for periodic open order requests
    std::atomic<int> nextValidOrderId{0}; ///< Next valid order ID from TWS

    std::mutex ordersMutex; ///< Mutex for thread-safe access to order status map
    std::unordered_map<int, std::string> knownOrderStatuses; ///< Map of order IDs to current status

    /**
     * @brief Constructor initializing signal timeout
     *
     * Sets up the OS signal with 100ms timeout for message processing.
     * Creates the EClientSocket bound to this monitor instance.
     */
    IBWrapperMonitor() : signal(100) {
        client = std::make_unique<EClientSocket>(this, &signal);
    }

    /**
     * @brief Destructor ensuring proper cleanup
     *
     * Disconnects from IB and joins all threads before destruction.
     * Ensures graceful shutdown of monitoring operations.
     */
    ~IBWrapperMonitor() override { disconnect(); }

    // ───────────────────────────────────────────────
    // Connection
    // ───────────────────────────────────────────────

    /**
     * @brief Establishes connection to IB Gateway/TWS
     *
     * @param host Server hostname or IP address (default: "127.0.0.1")
     * @param port Server port number (default: 4002 for IB Gateway)
     * @param clientId Unique client identifier (default: 2 for monitor)
     * @return true if connection successful, false otherwise
     *
     * Uses a different client ID (2) to avoid conflicts with main trading connection.
     * The actual message processing and polling threads are started in connectAck().
     */
    bool connect(const char* host = "127.0.0.1", int port = 4002, int clientId = 2) {
        if (!client->eConnect(host, port, clientId)) {
            LOG_ERROR("[Monitor] Failed to connect to IB Gateway/TWS.");
            return false;
        }
        LOG_INFO("[Monitor] Connected to IB Gateway (", host, ":", port, ", clientId=", clientId, ")");
        return true;
    }

    /**
     * @brief Callback invoked when connection is acknowledged
     *
     * Starts both the message reader thread and the periodic polling thread.
     * The reader thread processes incoming IB messages continuously, while the
     * polling thread requests all open orders every 3 seconds to detect status changes.
     * This dual-thread approach ensures both reactive (callbacks) and proactive (polling)
     * detection of order events.
     */
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

    /**
     * @brief Disconnects from IB and stops all threads
     *
     * Gracefully shuts down both reader and polling threads, then disconnects
     * from IB Gateway/TWS. Sets running flag to false to signal thread termination,
     * issues signal to wake up reader thread, and joins both threads before
     * completing disconnection.
     */
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

    /**
     * @brief Callback invoked when TWS provides the next valid order ID
     *
     * @param orderId Next valid order ID from TWS
     *
     * Updates the internal order ID counter. This is primarily informational
     * for the monitor since it doesn't place orders, only tracks them.
     */
    void nextValidId(OrderId orderId) override {
        nextValidOrderId = static_cast<int>(orderId);
        LOG_INFO("[Monitor] Next valid order ID received: ", nextValidOrderId);
    }

    /**
     * @brief Callback for each open order during periodic polling
     *
     * @param orderId Order identifier
     * @param contract Contract details of the order
     * @param order Order details including action, quantity, type, etc.
     * @param orderState Current order state including status
     *
     * Compares the new status with the previously known status for this order.
     * Only logs when a status change is detected to reduce noise. Thread-safe
     * update of the knownOrderStatuses map ensures concurrent access from
     * polling and callback threads doesn't cause race conditions.
     */
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

    /**
     * @brief Callback for order status updates
     *
     * @param orderId Order identifier
     * @param status Current order status string (e.g., "Submitted", "Filled", "Cancelled")
     * @param filled Quantity that has been filled
     * @param remaining Quantity still remaining to be filled
     * @param avgFillPrice Average fill price
     *
     * Tracks status changes and logs updates only when status differs from known state.
     * This prevents duplicate logging when status hasn't changed. Thread-safe via mutex
     * to ensure consistency when called concurrently with openOrder() callback.
     */
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

    /**
     * @brief Callback invoked when open orders transmission is complete
     *
     * Compares current order set with previous snapshot to detect cancelled or
     * closed orders that no longer appear in the open orders list. Orders that
     * existed in the previous snapshot but are missing from the current one are
     * logged as potentially cancelled or closed. The current order set becomes
     * the new baseline for the next polling cycle.
     */
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

    /**
     * @brief Callback for error messages
     *
     * @param id Request or order ID associated with error
     * @param time Timestamp (unused)
     * @param code Error code
     * @param msg Error message text
     *
     * Filters out benign informational messages to reduce log noise:
     * - Code 300: "Can't find EId" - harmless query errors
     * - Code 2104: Market data farm connection status
     * - Code 2107: HMDS data farm connection status
     * - Code 2158: Secure gateway connection status
     * - Code 2119: Market data farm reconnection
     *
     * All other errors are logged for investigation.
     */
    void error(int id, time_t, int code, const std::string& msg, const std::string&) override {
        if (code == 300 && msg.find("Can't find EId") != std::string::npos)
            return;
        if (code == 2104 || code == 2107 || code == 2158 || code == 2119)
            return;
        LOG_ERROR("[Monitor] Error [", code, "] ", msg);
    }
};

#endif  // QUANTDREAMCPP_IBWRAPPERMONITOR_H

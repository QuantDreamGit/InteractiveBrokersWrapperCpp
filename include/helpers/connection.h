#ifndef QUANTDREAMCPP_CONNECTION_H
#define QUANTDREAMCPP_CONNECTION_H

#include "logger.h"
#include "perf_timer.h"
#include "wrappers/IBWrapperBase.h"
#include <thread>
#include <chrono>

namespace IB::Helpers {

  inline bool ensureConnected(IBWrapperBase& ib,
                              const char* host = "127.0.0.1",
                              int port = 4002,
                              int clientId = 0)
  {
    return IB::Helpers::measure([&]() -> bool {
      int attempt = 0;

      while (true) {
        ++attempt;
        LOG_INFO("[IB] Attempting connection (try #", attempt, ")...");

        if (!ib.connect(host, port, clientId)) {
          LOG_DEBUG("[IB] Connection failed (attempt #", attempt, "), retrying...");
          continue;
        }

        // Wait for nextValidId as confirmation
        int waitMs = 0;
        while (ib.nextValidOrderId == -1 && waitMs < 5000) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          waitMs += 50;
        }

        if (ib.nextValidOrderId != -1) {
          LOG_INFO("[IB] Successfully connected to TWS/Gateway");
          return true;
        }

        LOG_ERROR("[IB] Connection established but no valid ID received — retrying...");
        ib.disconnect();
      }
    }, "ensureConnected");
  }

}  // namespace IB::Helpers

#endif  // QUANTDREAMCPP_CONNECTION_H

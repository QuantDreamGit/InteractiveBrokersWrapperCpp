#ifndef QUANTDREAMCPP_Connection_H
#define QUANTDREAMCPP_Connection_H

#include <chrono>
#include <thread>

#include "logger.h"
#include "open_markets.h"
#include "perf_timer.h"
#include "wrappers/IBBaseWrapper.h"

namespace IB::Helpers {
  template <typename T>
  requires std::is_base_of_v<IBBaseWrapper, T>
  inline bool ensureConnected(T& ib,
                              const char* host = "127.0.0.1",
                              int const port = 4002,
                              int const clientId = 0,
                              int const marketDataType = 1,  // 1 = real-time, 2 = frozen, 3 = delayed, 4 = delayed-frozen
                              const std::string& region = "US")
  {
    // Check market time first
    auto status = IB::Helpers::getMarketStatus(region);
    if (!status.isOpen) {
      auto nextT = std::chrono::system_clock::to_time_t(status.nextOpen);
      std::tm nextUTC{};
      gmtime_r(&nextT, &nextUTC);

      char buf[64];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &nextUTC);

      LOG_WARN("[IB] Market closed for region ", region,
               ". Next open at ", buf);
    }

    return IB::Helpers::measure([&]() -> bool {
      int attempt = 0;

      while (true) {
        ++attempt;
        LOG_SECTION("Connecting to TWS");
        LOG_INFO("[IB] [Connection] Attempting connection (try #", attempt, ")...");

        if (!ib.connect(host, port, clientId)) {
          LOG_WARN("[IB] [Connection] Retry in 2s...");
          std::this_thread::sleep_for(std::chrono::seconds(2));
          continue;
        }

        // Wait up to 8s for nextValidId
        int waitedMs = 0;
        while (ib.nextValidOrderId == -1 && waitedMs < 8000) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          waitedMs += 100;
        }

        if (ib.nextValidOrderId != -1) {
          LOG_INFO("[IB] [Connection] Connection verified (nextValidId=",
                   ib.nextValidOrderId.load(), ")");
          LOG_SECTION_END();

          ib.initializing = false;
          ib.client->reqMarketDataType(marketDataType);
          return true;
        }

        LOG_WARN("[IB] [Connection] No nextValidOrderId after 8s, reconnecting...");
        ib.disconnect();
        std::this_thread::sleep_for(std::chrono::seconds(2));
      }
    }, "ensureConnected");
  }
} // namespace IB::Helpers

#endif  // QUANTDREAMCPP_Connection_H

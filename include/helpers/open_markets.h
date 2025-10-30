//
// Created by user on 10/29/25.
//

#ifndef QUANTDREAMCPP_OPEN_MARKETS_H
#define QUANTDREAMCPP_OPEN_MARKETS_H
#include "wrappers/IBWrapperBase.h"
namespace IB::Helpers {

  inline bool isMarketOpen(IBWrapperBase& ib, const Contract& contract) {
    // Request contract details to get trading hours
    int reqId = ib.nextOrderId();
    auto details = IBWrapperBase::getSync<ContractDetails>(ib, reqId, [&]() {
        ib.client->reqContractDetails(reqId, contract);
    });

    const std::string& hoursStr = details.tradingHours;
    if (hoursStr.empty()) {
        LOG_WARN("[IB] No trading hours info for ", contract.symbol);
        return true; // fallback assume open
    }

    // Format example from IB:
    // "20251028:0930-1600;20251029:CLOSED;20251030:0930-1600"
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
  #ifdef _WIN32
      localtime_s(&local, &t);
  #else
      localtime_r(&t, &local);
  #endif

    char dateBuf[9];
    std::strftime(dateBuf, sizeof(dateBuf), "%Y%m%d", &local);
    std::string today(dateBuf);

    // Find today's entry
    size_t pos = hoursStr.find(today);
    if (pos == std::string::npos) {
        LOG_WARN("[IB] No trading hours for today in ", hoursStr);
        return true;
    }

    // Extract hours block for today
    auto segment = hoursStr.substr(pos, hoursStr.find(';', pos) - pos);
    if (segment.find("CLOSED") != std::string::npos) {
        LOG_INFO("[IB] Market CLOSED for ", contract.symbol);
        return false;
    }

    // Parse hours "20251028:0930-1600"
    auto colonPos = segment.find(':');
    auto dashPos = segment.find('-');
    if (colonPos == std::string::npos || dashPos == std::string::npos) {
        LOG_WARN("[IB] Unrecognized trading hours format: ", segment);
        return true;
    }

    std::string openStr = segment.substr(colonPos + 1, dashPos - colonPos - 1);
    std::string closeStr = segment.substr(dashPos + 1);

    int openH = std::stoi(openStr.substr(0, 2));
    int openM = std::stoi(openStr.substr(2, 2));
    int closeH = std::stoi(closeStr.substr(0, 2));
    int closeM = std::stoi(closeStr.substr(2, 2));

    int nowMins = local.tm_hour * 60 + local.tm_min;
    int openMins = openH * 60 + openM;
    int closeMins = closeH * 60 + closeM;

    bool open = (nowMins >= openMins && nowMins <= closeMins);
    LOG_INFO("[IB] Market ", (open ? "OPEN" : "CLOSED"),
             " for ", contract.symbol, " (", openStr, "-", closeStr, ")");
    return open;
  }

} // namespace IB::Helpers
#endif  // QUANTDREAMCPP_OPEN_MARKETS_H

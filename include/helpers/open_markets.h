//
// Created by user on 10/29/25.
//

#ifndef QUANTDREAMCPP_OPEN_MARKETS_H
#define QUANTDREAMCPP_OPEN_MARKETS_H
#include <chrono>
#include <ctime>

#include <chrono>
#include <ctime>
#include <string>
#include <iomanip>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>

namespace IB::Helpers {

struct MarketStatus {
    bool isOpen;
    std::chrono::system_clock::time_point nextOpen;
    std::chrono::minutes timeToOpen;
};

inline MarketStatus getMarketStatus(const std::string& region = "US") {
    using namespace std::chrono;
    MarketStatus status;

    // --- current UTC time ---
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_r(&t, &utc);

    // --- detect U.S. DST (approximation) ---
    int year = utc.tm_year + 1900;
    auto nth_weekday_of_month = [](int year, int month, int weekday, int nth) {
        std::tm t = {};
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = 1;
        std::mktime(&t);
        int firstWeekday = t.tm_wday;
        int day = 1 + ((7 + weekday - firstWeekday) % 7) + 7 * (nth - 1);
        t.tm_mday = day;
        std::mktime(&t);
        return t;
    };
    auto dstStart = nth_weekday_of_month(year, 3, 0, 2);  // 2nd Sunday in March
    auto dstEnd   = nth_weekday_of_month(year, 11, 0, 1); // 1st Sunday in Nov
    std::time_t nowT = std::mktime(&utc);
    bool isDST = (nowT >= std::mktime(&dstStart) && nowT < std::mktime(&dstEnd));

    // --- US market hours in UTC ---
    int openUTC = isDST ? 13 * 60 + 30 : 14 * 60 + 30;
    int closeUTC = isDST ? 20 * 60 : 21 * 60;

    int minutesUTC = utc.tm_hour * 60 + utc.tm_min;
    bool open = (minutesUTC >= openUTC && minutesUTC < closeUTC);
    status.isOpen = open;

    // --- compute next open time ---
    std::tm next = utc;
    int daysToAdd = 0;

    // Move to next weekday if weekend
    int wday = utc.tm_wday; // 0=Sun, 6=Sat
    if (wday == 6) daysToAdd = 2; // Sat -> Monday
    else if (wday == 0) daysToAdd = 1; // Sun -> Monday
    else if (minutesUTC >= closeUTC) daysToAdd = 1; // after close

    next.tm_mday += daysToAdd;
    next.tm_hour = openUTC / 60;
    next.tm_min  = openUTC % 60;
    next.tm_sec  = 0;
    status.nextOpen = system_clock::from_time_t(timegm(&next));

    // --- compute time-to-open (if closed) ---
    if (!open) {
        auto diff = duration_cast<minutes>(status.nextOpen - now);
        status.timeToOpen = diff < minutes(0) ? minutes(0) : diff;
    } else {
        status.timeToOpen = minutes(0);
    }

    // --- logging ---
    char buf[64];
    auto nextT = system_clock::to_time_t(status.nextOpen);
    std::tm nextUTC{};
    gmtime_r(&nextT, &nextUTC);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M UTC", &nextUTC);

    if (open) {
        LOG_INFO("[IB] US Market OPEN (09:30–16:00 ET, DST=", isDST, ")");
    } else {
        int hrs = status.timeToOpen.count() / 60;
        int mins = status.timeToOpen.count() % 60;
        LOG_INFO("[IB] US Market CLOSED — opens in ",
                 (hrs > 0 ? std::to_string(hrs) + "h" : ""),
                 std::setw(2),std::setfill('0'),mins,
                 "min (next open ", buf, ")");
    }

    return status;
}

} // namespace IB::Helpers


#endif  // QUANTDREAMCPP_OPEN_MARKETS_H

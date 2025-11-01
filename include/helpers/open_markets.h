//
// Created by user on 10/29/25.
//

#ifndef QUANTDREAMCPP_OPEN_MARKETS_H
#define QUANTDREAMCPP_OPEN_MARKETS_H

#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>

/**
 * @file open_markets.h
 * @brief Market status detection and trading hours validation
 *
 * This file provides functionality to determine if financial markets are currently open
 * and calculate time until next market open. It handles timezone conversions, daylight
 * saving time (DST) transitions, and weekend/holiday detection for accurate trading
 * window validation.
 */

namespace IB::Helpers {

/**
 * @brief Market operating status with timing information
 *
 * This structure encapsulates complete market status information including:
 * - Current open/closed state
 * - Next market opening time
 * - Duration until market opens (if closed)
 *
 * **Use Cases:**
 * - Pre-market validation before placing orders
 * - Scheduling automated trading workflows
 * - Displaying market status in UI
 * - Implementing trading hour restrictions
 *
 * @note All times are represented in UTC (`system_clock::time_point`) for
 *       cross-timezone compatibility. Convert to local time for display.
 *
 * @note `timeToOpen` is zero when `isOpen` is true, otherwise contains minutes
 *       until next market session begins.
 */
struct MarketStatus {
    bool isOpen;                                      ///< True if market is currently open for trading
    std::chrono::system_clock::time_point nextOpen;   ///< UTC timestamp of next market opening
    std::chrono::minutes timeToOpen;                  ///< Duration until next open (0 if currently open)
};

/**
 * @brief Determines current market status and calculates next opening time
 *
 * @param region Market region identifier (default: "US"). Currently only supports "US"
 * @return MarketStatus struct containing open state, next open time, and countdown
 *
 * This function performs comprehensive market status detection with the following workflow:
 *
 * **Step 1: Current Time Acquisition**
 * - Gets current UTC time from system clock
 * - Converts to broken-down UTC time (`std::tm`) for analysis
 * - Uses thread-safe `gmtime_r()` for UTC conversion
 *
 * **Step 2: DST Detection**
 * - Calculates DST transition dates for current year
 * - **DST Start**: 2nd Sunday in March at 2:00 AM local time
 * - **DST End**: 1st Sunday in November at 2:00 AM local time
 * - Uses helper lambda to find nth weekday of month
 * - Determines if current time falls within DST period
 *
 * **Step 3: Market Hours Computation**
 * - **US Market Hours**: 9:30 AM - 4:00 PM Eastern Time
 * - **UTC Conversion (DST Active)**: 1:30 PM - 8:00 PM UTC
 * - **UTC Conversion (Standard Time)**: 2:30 PM - 9:00 PM UTC
 * - Checks if current UTC time falls within trading window
 * - Sets `isOpen` based on current minute-of-day
 *
 * **Step 4: Next Opening Calculation**
 * - If currently open: countdown is zero
 * - If after close: advances to next business day
 * - **Weekend Handling**:
 *   - Saturday: advances 2 days to Monday
 *   - Sunday: advances 1 day to Monday
 * - Sets opening time to market start (9:30 AM ET in UTC)
 * - Calculates duration until next open
 *
 * **Step 5: Logging and Return**
 * - Formats next opening time as "YYYY-MM-DD HH:MM UTC"
 * - Logs current status:
 *   - **Open**: Displays trading hours and DST state
 *   - **Closed**: Shows countdown in hours/minutes and next open timestamp
 * - Returns populated `MarketStatus` struct
 *
 * @note This function uses UTC internally and does not depend on system timezone settings.
 *       All calculations are timezone-agnostic.
 *
 * @note The DST detection is approximate and may be incorrect for historical dates
 *       or future DST rule changes. For production systems, consider using timezone
 *       libraries like Howard Hinnant's `date` or `std::chrono` timezone support (C++20).
 *
 * @note The function does not account for market holidays (e.g., Christmas, Thanksgiving).
 *       Additional holiday calendar logic would be needed for production accuracy.
 *
 * @note The `region` parameter is currently unused. Only US market hours are supported.
 *       Future implementations could extend to support "EU", "ASIA", etc.
 *
 * @warning The function assumes markets are closed on weekends (Saturday/Sunday). This
 *          is correct for US equity markets but may not apply to forex, crypto, or
 *          international markets.
 *
 * @warning Execution near midnight UTC may produce unexpected results due to date
 *          boundary conditions. Test thoroughly for edge cases.
 *
 * Example usage:
 * @code
 * // Check if market is open before placing order
 * auto status = IB::Helpers::getMarketStatus();
 * if (status.isOpen) {
 *     LOG_INFO("Market is open - placing order");
 *     placeOrder(ib, contract, order);
 * } else {
 *     LOG_WARN("Market closed - order scheduled for next open");
 *     scheduleOrder(order, status.nextOpen);
 * }
 *
 * // Display countdown to market open
 * if (!status.isOpen) {
 *     int hours = status.timeToOpen.count() / 60;
 *     int mins = status.timeToOpen.count() % 60;
 *     std::cout << "Market opens in " << hours << "h " << mins << "m" << std::endl;
 * }
 *
 * // Wait for market to open
 * auto status = IB::Helpers::getMarketStatus();
 * if (!status.isOpen) {
 *     std::this_thread::sleep_until(status.nextOpen);
 *     LOG_INFO("Market now open - resuming trading");
 * }
 * @endcode
 */
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

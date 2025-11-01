#ifndef QUANTDREAMCPP_IBSECTYPE_H
#define QUANTDREAMCPP_IBSECTYPE_H

#include <stdexcept>
#include <string>
#include <unordered_map>

/**
 * @file IBSecType.h
 * @brief Security type enumeration and conversion utilities for Interactive Brokers API
 *
 * This file provides a type-safe enumeration of all IB security types (secType field) with
 * bidirectional conversion between C++ enums and IB API string representations. It enables
 * compile-time checking while maintaining compatibility with IB's string-based protocol.
 *
 * **Key Features:**
 * - Complete coverage of all 14 IB security types
 * - Zero-overhead enum-to-string conversion
 * - Fast string-to-enum lookup via hash map
 * - Exception-based error handling for invalid strings
 */

namespace IB {

  /**
   * @brief Enumerates all valid Interactive Brokers security types (secType)
   *
   * This enum provides a type-safe representation of IB's security type classifications.
   * Each value corresponds to a specific asset class and maps to an IB API string code
   * (see `to_string()` for the mapping table).
   *
   * **Security Type Categories:**
   *
   * **Equity Instruments:**
   * - `STOCK`: Common stocks, preferred shares, ADRs
   * - `WARRANT`: Exchange-traded warrants on stocks
   * - `FUND`: Mutual funds, ETFs, unit trusts
   *
   * **Derivative Instruments:**
   * - `OPTION`: Equity or index options (calls/puts)
   * - `FOP`: Options on futures contracts
   * - `FUTURE`: Futures contracts (commodities, indices, currencies)
   *
   * **Fixed Income:**
   * - `BOND`: Corporate bonds, government bonds, treasuries
   *
   * **Foreign Exchange:**
   * - `FOREX`: Currency pairs (e.g., EUR/USD)
   *
   * **Alternative Products:**
   * - `CFD`: Contracts for difference
   * - `CMDTY`: Physical commodities (gold, oil, etc.)
   * - `ICMD`: Commodity indices
   *
   * **Reference Data:**
   * - `INDEX`: Stock indices (S&P 500, NASDAQ, etc.)
   * - `NEWS`: News ticker symbols
   *
   * **Complex Structures:**
   * - `COMBO`: Multi-leg spreads, baskets, combos
   *
   * @note The IB API uses 3-4 letter string codes internally (e.g., "STK" for stocks).
   *       Use `to_string()` to convert enum values for API calls.
   *
   * @note Not all security types are available on all exchanges. Check IB's product
   *       listings for regional availability.
   *
   * Example usage:
   * @code
   * // Create a stock contract
   * Contract contract;
   * contract.secType = IB::to_string(IB::SecType::STOCK);  // "STK"
   * contract.symbol = "AAPL";
   *
   * // Parse incoming contract data
   * IB::SecType type = IB::from_string(contract.secType);  // STOCK
   * if (type == IB::SecType::OPTION) {
   *     // Handle option-specific logic
   * }
   * @endcode
   */
  enum class SecType {
    STOCK,     ///< Stock / Equity (IB code: "STK")
    OPTION,    ///< Option on stock or index (IB code: "OPT")
    FUTURE,    ///< Futures contract (IB code: "FUT")
    INDEX,     ///< Index (IB code: "IND")
    FOREX,     ///< Currency pair / Forex (IB code: "CASH")
    CFD,       ///< Contract for difference (IB code: "CFD")
    FOP,       ///< Option on a future (IB code: "FOP")
    BOND,      ///< Corporate or government bond (IB code: "BOND")
    FUND,      ///< Mutual fund or ETF (IB code: "FUND")
    WARRANT,   ///< Exchange-traded warrant (IB code: "WAR")
    COMBO,     ///< Combo / multi-leg spread (IB code: "BAG")
    CMDTY,     ///< Physical commodity (IB code: "CMDTY")
    NEWS,      ///< News ticker (IB code: "NEWS")
    ICMD       ///< Commodity index (IB code: "ICMD")
  };

  /**
   * @brief Converts a SecType enum to its corresponding IB API string representation
   *
   * @param t The SecType enum value to convert
   * @return IB API string code (e.g., "STK", "OPT", "FUT")
   *
   * This function performs a direct enum-to-string mapping using a switch statement,
   * providing zero runtime overhead. All valid enum values are guaranteed to map to
   * their correct IB API codes.
   *
   * **Conversion Table:**
   * | Enum Value      | IB API Code | Description                    |
   * |-----------------|-------------|--------------------------------|
   * | `STOCK`         | `"STK"`     | Stock / Equity                 |
   * | `OPTION`        | `"OPT"`     | Option on stock or index       |
   * | `FUTURE`        | `"FUT"`     | Futures contract               |
   * | `INDEX`         | `"IND"`     | Index                          |
   * | `FOREX`         | `"CASH"`    | Currency pair (Forex)          |
   * | `CFD`           | `"CFD"`     | Contract for difference        |
   * | `FOP`           | `"FOP"`     | Option on a future             |
   * | `BOND`          | `"BOND"`    | Corporate or government bond   |
   * | `FUND`          | `"FUND"`    | Mutual fund or ETF             |
   * | `WARRANT`       | `"WAR"`     | Exchange-traded warrant        |
   * | `COMBO`         | `"BAG"`     | Combo / multi-leg spread       |
   * | `CMDTY`         | `"CMDTY"`   | Physical commodity             |
   * | `NEWS`          | `"NEWS"`    | News ticker                    |
   * | `ICMD`          | `"ICMD"`    | Commodity index                |
   *
   * @note The function returns `"UNKNOWN"` only if an invalid enum value is passed
   *       (which should never occur with proper enum usage).
   *
   * @note This is an `inline` function optimized for zero overhead in release builds.
   *       The compiler will typically eliminate the switch statement entirely.
   *
   * Example usage:
   * @code
   * // Create stock contract
   * Contract stockContract;
   * stockContract.secType = IB::to_string(IB::SecType::STOCK);  // "STK"
   * stockContract.symbol = "AAPL";
   *
   * // Create option contract
   * Contract optionContract;
   * optionContract.secType = IB::to_string(IB::SecType::OPTION);  // "OPT"
   * optionContract.symbol = "AAPL";
   * optionContract.strike = 150.0;
   *
   * // Generic conversion
   * IB::SecType type = IB::SecType::FUTURE;
   * std::string ibCode = IB::to_string(type);  // "FUT"
   * @endcode
   */
  inline std::string to_string(SecType t) {
    switch (t) {
      case SecType::STOCK:   return "STK";
      case SecType::OPTION:  return "OPT";
      case SecType::FUTURE:  return "FUT";
      case SecType::INDEX:   return "IND";
      case SecType::FOREX:   return "CASH";
      case SecType::CFD:     return "CFD";
      case SecType::FOP:     return "FOP";
      case SecType::BOND:    return "BOND";
      case SecType::FUND:    return "FUND";
      case SecType::WARRANT: return "WAR";
      case SecType::COMBO:   return "BAG";
      case SecType::CMDTY:   return "CMDTY";
      case SecType::NEWS:    return "NEWS";
      case SecType::ICMD:    return "ICMD";
    }
    return "UNKNOWN";  // Should never reach here with valid enum
  }

  /**
   * @brief Converts an IB secType string (like "STK") to a SecType enum
   *
   * @param s The IB API security type string to convert
   * @return The corresponding SecType enum value
   * @throws std::invalid_argument if the input string is not a recognized IB security type
   *
   * This function performs reverse lookup from IB API string codes to the type-safe enum.
   * It uses a static hash map for O(1) lookup performance with minimal memory overhead.
   *
   * **Conversion Process:**
   * 1. Static hash map is initialized on first call (thread-safe in C++11+)
   * 2. Input string is looked up in the map
   * 3. If found, returns corresponding enum value
   * 4. If not found, throws `std::invalid_argument` with descriptive error message
   *
   * **Valid Input Strings:**
   * - `"STK"` → `SecType::STOCK`
   * - `"OPT"` → `SecType::OPTION`
   * - `"FUT"` → `SecType::FUTURE`
   * - `"IND"` → `SecType::INDEX`
   * - `"CASH"` → `SecType::FOREX`
   * - `"CFD"` → `SecType::CFD`
   * - `"FOP"` → `SecType::FOP`
   * - `"BOND"` → `SecType::BOND`
   * - `"FUND"` → `SecType::FUND`
   * - `"WAR"` → `SecType::WARRANT`
   * - `"BAG"` → `SecType::COMBO`
   * - `"CMDTY"` → `SecType::CMDTY`
   * - `"NEWS"` → `SecType::NEWS`
   * - `"ICMD"` → `SecType::ICMD`
   *
   * @note The lookup is **case-sensitive**. Lowercase strings like "stk" will throw an exception.
   *
   * @note The static hash map has minimal overhead (~14 entries × 16 bytes ≈ 224 bytes) and
   *       is shared across all calls, making this function very efficient.
   *
   * @warning Always wrap calls in try-catch blocks when parsing untrusted input:
   *          ```cpp
   *          try {
   *              auto type = IB::from_string(unknownString);
   *          } catch (const std::invalid_argument& e) {
   *              LOG_ERROR("Invalid security type: ", e.what());
   *          }
   *          ```
   *
   * Example usage:
   * @code
   * // Parse IB API response
   * void contractDetails(int reqId, const ContractDetails& details) {
   *     try {
   *         IB::SecType type = IB::from_string(details.contract.secType);
   *
   *         switch (type) {
   *             case IB::SecType::STOCK:
   *                 LOG_INFO("Received stock contract");
   *                 break;
   *             case IB::SecType::OPTION:
   *                 LOG_INFO("Received option contract");
   *                 break;
   *             default:
   *                 LOG_WARN("Unexpected security type");
   *         }
   *     } catch (const std::invalid_argument& e) {
   *         LOG_ERROR("Invalid secType in contract: ", e.what());
   *     }
   * }
   *
   * // Validate user input
   * std::string userInput = "FUT";
   * IB::SecType type = IB::from_string(userInput);  // SecType::FUTURE
   *
   * // Handle invalid input
   * try {
   *     IB::from_string("INVALID");  // Throws exception
   * } catch (const std::invalid_argument& e) {
   *     std::cerr << e.what() << std::endl;  // "Invalid secType: INVALID"
   * }
   * @endcode
   */
  inline SecType from_string(const std::string& s) {
    // Static hash map initialized once (thread-safe in C++11+)
    static const std::unordered_map<std::string, SecType> map = {
      {"STK", SecType::STOCK},
      {"OPT", SecType::OPTION},
      {"FUT", SecType::FUTURE},
      {"IND", SecType::INDEX},
      {"CASH", SecType::FOREX},
      {"CFD", SecType::CFD},
      {"FOP", SecType::FOP},
      {"BOND", SecType::BOND},
      {"FUND", SecType::FUND},
      {"WAR", SecType::WARRANT},
      {"BAG", SecType::COMBO},
      {"CMDTY", SecType::CMDTY},
      {"NEWS", SecType::NEWS},
      {"ICMD", SecType::ICMD}
    };

    auto it = map.find(s);
    if (it == map.end())
      throw std::invalid_argument("Invalid secType: " + s);
    return it->second;
  }

} // namespace IB

#endif  // QUANTDREAMCPP_IBSECTYPE_H

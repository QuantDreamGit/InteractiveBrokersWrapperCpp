#ifndef QUANTDREAMCPP_IBSECTYPE_H
#define QUANTDREAMCPP_IBSECTYPE_H

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace IB {

  /// @brief Enumerates all valid Interactive Brokers security types (secType).
  enum class SecType {
    STOCK,     ///< Stock / Equity
    OPTION,    ///< Option on stock or index
    FUTURE,    ///< Futures contract
    INDEX,     ///< Index
    FOREX,     ///< Currency pair (Forex)
    CFD,       ///< Contract for difference
    FOP,       ///< Option on a future
    BOND,      ///< Corporate or government bond
    FUND,      ///< Mutual fund or ETF
    WARRANT,   ///< Exchange-traded warrant
    COMBO,     ///< Combo / multi-leg spread
    CMDTY,     ///< Physical commodity
    NEWS,      ///< News ticker
    ICMD       ///< Commodity index
  };

  /// Converts a SecType enum to its corresponding IB API string representation.
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
    return "UNKNOWN";
  }

  /// Converts an IB secType string (like "STK") to a SecType enum.
  /// Throws std::invalid_argument if invalid.
  inline SecType from_string(const std::string& s) {
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

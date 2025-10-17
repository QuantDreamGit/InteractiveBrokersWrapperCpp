//
// Created by user on 10/15/25.
//

#ifndef QUANTDREAMCPP_MARKETDATAREQUESTS_H
#define QUANTDREAMCPP_MARKETDATAREQUESTS_H
#include <memory>

#include "EClientSocket.h"

namespace IB::Requests {

  /** Request market data for a given contract
   *
   * @param client        Pointer to the EClientSocket instance
   * @param dataType      Market data type: 1 = real-time, 2 = frozen, 3 = delayed, 4 = delayed-frozen
   * @param contract      Contract for which to request market data
   * @param tickerId      Ticker ID for the market data request (default: 1001)
   * @param genericTicks  Comma-separated list of generic tick types to request (default: "")
   * @param snapshot      If true, requests a snapshot of the market data (default: false)
   * @param regulatorySnapshot If true, requests a regulatory snapshot (default: false)
   */
  inline void requestMarketData(std::unique_ptr<EClientSocket> const& client,
                                int const dataType,
                                Contract const& contract,
                                TickerId const tickerId = 1001,
                                std::string const& genericTicks = "",
                                bool const snapshot = false,
                                bool const regulatorySnapshot = false) {
    // Check if client is connected
    if (!client || !client->isConnected()) {
      std::cerr << "[IB] Cannot request market data: not connected." << std::endl;
      return;
    }

    // Set market data type: 1 = real-time, 2 = frozen, 3 = delayed, 4 = delayed-frozen
    // Check if dataType is valid
    if (dataType < 1 || dataType > 4) {
      std::cerr << "[IB] Invalid market data type: " << dataType << ". Must be 1, 2, 3, or 4." << std::endl;
      return;
    }

    // Request market data
    client->reqMarketDataType(dataType);
    client->reqMktData(tickerId,
                       contract,
                       genericTicks,
                       snapshot, regulatorySnapshot,
                       TagValueListSPtr());
  }
}

#endif  // QUANTDREAMCPP_MARKETDATAREQUESTS_H

//
// Created by user on 10/17/25.
//

#ifndef QUANTDREAMCPP_CONTRACTDETAILS_H
#define QUANTDREAMCPP_CONTRACTDETAILS_H

#include "Contract.h"
#include "EClientSocket.h"
#include "IBRequestIds.h"
#include "IBSecType.h"
#include "helpers/logger.h"
#include "helpers/perf_timer.h"
#include "wrappers/IBBaseWrapper.h"

/**
 * @file ContractDetails.h
 * @brief Contract details retrieval functionality for Interactive Brokers API
 *
 * This file provides a templated function to request and synchronously retrieve
 * detailed contract information from IB Gateway/TWS. It uses the promise-based
 * synchronization mechanism from IBBaseWrapper to block until contract details
 * are received.
 */

namespace IB::Requests {
  /**
   * @brief Synchronously retrieves contract details from Interactive Brokers
   *
   * @tparam T Wrapper type that must inherit from IBBaseWrapper
   * @param ib Reference to the IBBaseWrapper instance (or derived class)
   * @param contract Contract specification to look up (symbol, exchange, etc.)
   * @param reqId Request identifier for tracking the request (default: BASE_CONTRACT_ID)
   * @return Contract object with populated details from IB
   *
   * This function requests contract details from IB using reqContractDetails() and
   * synchronously waits for the response using the promise-future mechanism in
   * IBBaseWrapper::getSync(). It automatically handles:
   * - Request submission via EClientSocket
   * - Promise creation and registration
   * - Callback handling (contractDetails() in wrapper)
   * - Timeout detection (if configured)
   * - Performance measurement logging
   *
   * The function blocks the calling thread until contract details are received or
   * a timeout occurs. Upon successful retrieval, it logs summary information including
   * symbol, contract ID, and exchange.
   *
   * @note This function requires that the wrapper's contractDetails() callback is
   *       properly implemented to fulfill the promise with Contract data.
   *
   * @throws std::runtime_error if the request times out or fails
   *
   * Example usage:
   * @code
   * Contract contract;
   * contract.symbol = "AAPL";
   * contract.secType = "STK";
   * contract.exchange = "SMART";
   * contract.currency = "USD";
   *
   * Contract details = IB::Requests::getContractDetails(ib, contract);
   * std::cout << "ConId: " << details.conId << std::endl;
   * @endcode
   */
  template <typename T>
  requires std::is_base_of_v<IBBaseWrapper, T>
  inline Contract getContractDetails(T& ib,
                                     const Contract& contract,
                                     int reqId = IB::ReqId::BASE_CONTRACT_ID)
  {
    return IB::Helpers::measure([&]() -> Contract {

      // Submit contract details request and wait synchronously for response
      auto contractDetails = IBBaseWrapper::getSync<Contract>(ib, reqId, [&]() {
        ib.client->reqContractDetails(reqId, contract);
      });

      // Log summary of received contract details
      LOG_DEBUG("[IB] Contract details received: ",
                contractDetails.symbol,
                " (ConId: ", contractDetails.conId,
                ") on ", contractDetails.exchange);

      return contractDetails;

    }, "getContractDetails");
  }
}  // namespace IB::Requests

#endif  // QUANTDREAMCPP_CONTRACTDETAILS_H

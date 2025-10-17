//
// Created by user on 10/17/25.
//

#ifndef QUANTDREAMCPP_CONTRACTDETAILS_H
#define QUANTDREAMCPP_CONTRACTDETAILS_H

#include "wrappers/IBWrapperBase.h"
#include "Contract.h"
#include "EClientSocket.h"
#include "IBRequestIds.h"
#include "IBSecType.h"
#include "contracts/StockContracts.h"
#include "data_structures/contracts.h"


namespace IB::Requests {
  inline Contract getContractDetails(IBWrapperBase& ib,
                              Contract const& contract,
                              int const reqId = IB::ReqId::BASE_CONTRACT_ID) {

    // Create a promise and future to get the contract details asynchronously
    std::promise<Contract> contractPromise;
    std::future<Contract> contractFuture = contractPromise.get_future();

    {
      std::lock_guard<std::mutex> lock(ib.promiseMutex);
      // Store the promise in the map with the request ID
      ib.contractDetailsPromises[reqId] = std::move(contractPromise);
    }

    // Request contract details from IB TWS
    ib.client -> reqContractDetails(reqId, contract);

    // Wait for the future to be fulfilled and get the contract details
    Contract contractDetails = contractFuture.get();

    // Print summary of the received contract details
    std::cout << "Contract details received: "
    << contractDetails.symbol << " (ConId: " << contractDetails.conId << ")"
    << " on " << contractDetails.exchange << std::endl;

    return contractDetails;
  }
}
#endif  // QUANTDREAMCPP_CONTRACTDETAILS_H

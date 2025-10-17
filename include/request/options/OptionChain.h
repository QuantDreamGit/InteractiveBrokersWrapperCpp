//
// Created by user on 10/17/25.
//

#ifndef QUANTDREAMCPP_OPTIONCHAIN_H
#define QUANTDREAMCPP_OPTIONCHAIN_H

#include "IBRequestIds.h"
#include "request/contracts/ContractDetails.h"
#include "wrappers/IBWrapperBase.h"

namespace IB::Request {
  inline IB::Options::ChainInfo getOptionChain(IBWrapperBase& ib,
                                               Contract const& contract,
                                               int const reqId = IB::ReqId::OPTION_CHAIN_ID) {

    // Request contract id for the underlying symbol
    Contract contractDetails = Requests::getContractDetails(ib, contract, IB::ReqId::BASE_CONTRACT_ID);
    int underlyingConId = contractDetails.conId;

    // Create a promise and future to get the option chain asynchronously
    std::promise<IB::Options::ChainInfo> optionChainPromise;
    auto optionChainFuture = optionChainPromise.get_future();

    {
      std::lock_guard<std::mutex> lock(ib.promiseMutex);
      // Store the promise in the map with the request ID
      ib.optionChainPromises[reqId] = std::move(optionChainPromise);
    }

    // Send the request for option chain parameters
    ib.client -> reqSecDefOptParams(reqId,
                                   contract.symbol,
                                   "",
                                   contract.secType,
                                   underlyingConId);

    // Wait for the future to be fulfilled and get the option chain
    IB::Options::ChainInfo optionChain = optionChainFuture.get();

    // Print summary of the received option chain
    std::cout << "Option chain received: "
          << optionChain.tradingClass << " on " << optionChain.exchange
          << " (" << optionChain.expirations.size() << " expirations, "
          << optionChain.strikes.size() << " strikes)" << std::endl;

    return optionChain;
  }
}
#endif  // QUANTDREAMCPP_OPTIONCHAIN_H

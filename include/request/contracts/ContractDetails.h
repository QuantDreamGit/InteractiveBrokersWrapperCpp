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

namespace IB::Requests {
  template <typename T>
  requires std::is_base_of_v<IBBaseWrapper, T>
  inline Contract getContractDetails(T& ib,
                                     const Contract& contract,
                                     int reqId = IB::ReqId::BASE_CONTRACT_ID)
  {
    return IB::Helpers::measure([&]() -> Contract {

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

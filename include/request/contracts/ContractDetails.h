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
#include "helpers/perf_timer.h"
#include "helpers/logger.h"

namespace IB::Requests {

  inline Contract getContractDetails(IBWrapperBase& ib,
                                     const Contract& contract,
                                     int reqId = IB::ReqId::BASE_CONTRACT_ID)
  {
    return IB::Helpers::measure([&]() -> Contract {

      auto contractDetails = IBWrapperBase::getSync<Contract>(ib, reqId, [&]() {
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

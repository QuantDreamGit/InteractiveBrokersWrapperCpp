//
// Created by user on 10/17/25.
//

#ifndef QUANTDREAMCPP_IBREQUESTIDS_H
#define QUANTDREAMCPP_IBREQUESTIDS_H
namespace IB::ReqId {

  // Request IDs for contracts
  constexpr int BASE_CONTRACT_ID = 1000;
  constexpr int STOCK_CONTRACT_ID = 1001;
  constexpr int OPTION_CONTRACT_ID = 1002;
  constexpr int FUTURE_CONTRACT_ID = 1003;

  // Request IDs for option chains
  constexpr int OPTION_CHAIN_ID = 2001;


}
#endif  // QUANTDREAMCPP_IBREQUESTIDS_H

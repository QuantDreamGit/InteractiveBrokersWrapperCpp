//
// Created by user on 10/17/25.
//

#ifndef QUANTDREAMCPP_IBREQUESTIDS_H
#define QUANTDREAMCPP_IBREQUESTIDS_H
namespace IB::ReqId {

  // Orders base ID
  constexpr int BASE_ORDER_ID = 0;
  constexpr int OPEN_ORDER_ID = 100;
  constexpr int ALL_OPEN_ORDER_ID = 200;
  constexpr int CANCEL_ORDER_ID = 300;
  constexpr int CANCEL_ALL_ORDER_ID = 400;

  // Request IDs for contracts
  constexpr int BASE_CONTRACT_ID = 1000;
  constexpr int STOCK_CONTRACT_ID = 1100;
  constexpr int OPTION_CONTRACT_ID = 1200;
  constexpr int FUTURE_CONTRACT_ID = 1300;
  constexpr int OPEN_MARKET_CONTRACT_ID = 1400;

  // Request IDs for option chains
  constexpr int OPTION_CHAIN_ID = 2000;
  constexpr int OPTION_CHAIN_GREEKS_ID = 2100;

  // Request IDs for market data
  constexpr int MARKET_DATA_ID = 3000;

  constexpr int SNAPSHOT_DATA_ID = 4000;

  constexpr int POSITION_ID = 5000;




}
#endif  // QUANTDREAMCPP_IBREQUESTIDS_H

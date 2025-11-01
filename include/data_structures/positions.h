//
// Created by user on 10/30/25.
//

#ifndef QUANTDREAMCPP_POSITIONS_H
#define QUANTDREAMCPP_POSITIONS_H
#include <string>

#include "Contract.h"

namespace IB::Accounts {
  struct PositionInfo {
    std::string account;
    Contract contract;
    double position = 0.0;
    double avgCost = 0.0;
  };
}

#endif  // QUANTDREAMCPP_POSITIONS_H

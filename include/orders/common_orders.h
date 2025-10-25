//
// Created by user on 10/24/25.
//

#ifndef QUANTDREAMCPP_COMMON_ORDERS_H
#define QUANTDREAMCPP_COMMON_ORDERS_H
#include "Order.h"    // from IB API
#include <string>
#include "Decimal.h"

namespace IB::Orders {

  // --- BASIC ORDER BUILDERS ---

  inline Order MarketBuy(long quantity, bool transmit = true) {
    Order o;
    o.action        = "BUY";
    o.totalQuantity = DecimalFunctions::doubleToDecimal(static_cast<double>(quantity));
    o.orderType     = "MKT";
    o.tif           = "DAY";
    o.transmit      = transmit;
    return o;
  }

  inline Order MarketSell(int quantity, bool transmit = true) {
    Order o;
    o.action        = "SELL";
    o.totalQuantity = DecimalFunctions::doubleToDecimal(static_cast<double>(quantity));
    o.orderType     = "MKT";
    o.tif           = "DAY";
    o.transmit      = transmit;
    return o;
  }

  inline Order LimitBuy(int quantity, double price, bool transmit = true) {
    Order o;
    o.action        = "BUY";
    o.totalQuantity = DecimalFunctions::doubleToDecimal(static_cast<double>(quantity));
    o.orderType     = "LMT";
    o.lmtPrice      = price;
    o.tif           = "DAY";
    o.transmit      = transmit;
    return o;
  }

  inline Order LimitSell(int quantity, double price, bool transmit = true) {
    Order o;
    o.action        = "SELL";
    o.totalQuantity = DecimalFunctions::doubleToDecimal(static_cast<double>(quantity));
    o.orderType     = "LMT";
    o.lmtPrice      = price;
    o.tif           = "DAY";
    o.transmit      = transmit;
    return o;
  }

  inline Order StopSell(int quantity, double stopPrice, bool transmit = true) {
    Order o;
    o.action        = "SELL";
    o.totalQuantity = DecimalFunctions::doubleToDecimal(static_cast<double>(quantity));
    o.orderType     = "STP";
    o.auxPrice      = stopPrice;
    o.tif           = "GTC";
    o.transmit      = transmit;
    return o;
  }

  inline Order StopLimitSell(int quantity, double stopPrice, double limitPrice, bool transmit = true) {
    Order o;
    o.action        = "SELL";
    o.totalQuantity = DecimalFunctions::doubleToDecimal(static_cast<double>(quantity));
    o.orderType     = "STP LMT";
    o.auxPrice      = stopPrice;  // trigger
    o.lmtPrice      = limitPrice; // execution
    o.tif           = "GTC";
    o.transmit      = transmit;
    return o;
  }

  inline Order TrailingStopSell(int quantity, double trailAmount, bool transmit = true) {
    Order o;
    o.action        = "SELL";
    o.totalQuantity = DecimalFunctions::doubleToDecimal(static_cast<double>(quantity));
    o.orderType     = "TRAIL";
    o.auxPrice      = trailAmount; // trailing offset
    o.tif           = "GTC";
    o.transmit      = transmit;
    return o;
  }

  // --- COMBO ORDERS ---
  inline Order ComboEntry(double limitPrice, bool transmit = false) {
    Order o;
    o.action        = "BUY";
    o.orderType     = "LMT";
    o.totalQuantity = DecimalFunctions::doubleToDecimal(static_cast<double>(1));
    o.lmtPrice      = limitPrice;
    o.tif           = "DAY";
    o.transmit      = transmit;   // false until final leg added
    return o;
  }

  inline Order ComboExit(double limitPrice, bool transmit = false) {
    Order o;
    o.action        = "SELL";
    o.orderType     = "LMT";
    o.totalQuantity = DecimalFunctions::doubleToDecimal(static_cast<double>(1));
    o.lmtPrice      = limitPrice;
    o.tif           = "DAY";
    o.transmit      = transmit;
    return o;
  }

} // namespace IB::Orders
#endif  // QUANTDREAMCPP_COMMON_ORDERS_H

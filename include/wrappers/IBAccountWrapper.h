#ifndef QUANTDREAMCPP_IBACCOUNTWRAPPER_H
#define QUANTDREAMCPP_IBACCOUNTWRAPPER_H

#include "IBBaseWrapper.h"

/**
 * @file IBAccountWrapper.h
 * @brief Account management and position tracking for Interactive Brokers API
 *
 * This file provides functionality for retrieving account summaries and position information
 * from Interactive Brokers TWS. It handles callbacks for account data and maintains position buffers.
 */

/**
 * @class IBAccountWrapper
 * @brief Handles account summary and position management.
 *
 * Extends IBBaseWrapper to provide specialized handling for account-related callbacks
 * including account summaries, positions, and related data from the IB API.
 */
class IBAccountWrapper : public virtual IBBaseWrapper {
public:
  /**
   * @brief Callback invoked when account summary data is received
   *
   * @param reqId Request identifier for the account summary request
   * @param account Account identifier (e.g., account number)
   * @param tag Type of account data (e.g., "TotalCashValue", "NetLiquidation")
   * @param value The value associated with the tag
   * @param currency Currency denomination for the value
   *
   * Logs account summary information for debugging purposes.
   */
  void accountSummary(int reqId, const std::string& account,
                      const std::string& tag, const std::string& value,
                      const std::string& currency) override {
    LOG_DEBUG("[AccountSummary] ", account, " ", tag, " = ", value, " ", currency);
  }

  /**
   * @brief Callback invoked when all account summary data has been received
   *
   * @param reqId Request identifier for the completed account summary request
   *
   * Signals the completion of account summary data transmission.
   */
  void accountSummaryEnd(int reqId) override {
    LOG_DEBUG("[AccountSummaryEnd] reqId=", reqId);
  }

  /**
   * @brief Callback invoked for each position held in the account
   *
   * @param account Account identifier holding the position
   * @param contract Contract details of the position
   * @param position Position size (positive for long, negative for short)
   * @param avgCost Average cost basis of the position
   *
   * Creates PositionInfo objects and buffers them for later retrieval.
   * Ignores zero positions.
   */
  void position(const std::string& account, const Contract& contract,
                Decimal position, double avgCost) override {
    double pos = DecimalFunctions::decimalToDouble(position);
    if (pos == 0.0) return;

    IB::Accounts::PositionInfo info{account, contract, pos, avgCost};
    positionBuffer.push_back(info);

    LOG_DEBUG("[Position] ", contract.symbol, " ", contract.secType,
             " ", (pos > 0 ? "LONG " : "SHORT "),
             std::abs(pos),
             " @ avgCost=", avgCost);
  }

  /**
   * @brief Callback invoked when all position data has been received
   *
   * Fulfills the promise with the collected position buffer and clears it.
   * This signals completion of a position data request.
   */
  void positionEnd() override {
    LOG_DEBUG("[PositionEnd] Finished receiving positions.");
    fulfillPromise<std::vector<IB::Accounts::PositionInfo>>(IB::ReqId::POSITION_ID, positionBuffer);
    positionBuffer.clear();
  }
};

#endif

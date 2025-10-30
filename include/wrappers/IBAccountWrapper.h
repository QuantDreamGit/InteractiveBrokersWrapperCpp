#ifndef QUANTDREAMCPP_IBACCOUNTWRAPPER_H
#define QUANTDREAMCPP_IBACCOUNTWRAPPER_H

#include "IBBaseWrapper.h"

/**
 * @class IBAccountWrapper
 * @brief Handles account summary and position management.
 */
class IBAccountWrapper : public virtual IBBaseWrapper {
public:
  void accountSummary(int reqId, const std::string& account,
                      const std::string& tag, const std::string& value,
                      const std::string& currency) override {
    LOG_DEBUG("[AccountSummary] ", account, " ", tag, " = ", value, " ", currency);
  }

  void accountSummaryEnd(int reqId) override {
    LOG_DEBUG("[AccountSummaryEnd] reqId=", reqId);
  }

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

  void positionEnd() override {
    LOG_DEBUG("[PositionEnd] Finished receiving positions.");
    fulfillPromise<std::vector<IB::Accounts::PositionInfo>>(IB::ReqId::POSITION_ID, positionBuffer);
    positionBuffer.clear();
  }
};

#endif

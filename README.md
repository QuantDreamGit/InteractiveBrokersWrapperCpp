# Interactive Brokers C++ Wrapper

Interactive Brokers Wrapper C++ is a header-first static library that layers modern C++ utilities on top of the official Interactive Brokers (IBKR) C++ API. The project provides a convenient starting point for managing the socket connection lifecycle, requesting contract metadata, and querying option chain definitions while keeping compatibility with the upstream `EClientSocket` interface.

## Features

- **Managed client lifecycle** – `IBWrapperBase` boots an `EClientSocket`, starts an `EReader` thread when the connection handshake completes, and exposes promise-based maps for delivering asynchronous contract and option-chain responses.【F:include/wrappers/IBWrapperBase.h†L16-L235】
- **Rich option chain model** – `IB::Options::ChainInfo` captures exchange, trading class, multiplier, expirations, and strikes returned from `securityDefinitionOptionParameter`, making it easy to inspect available expiries and strikes before creating individual option contracts.【F:include/data_structures/options.h†L10-L42】
- **Request helpers** – Inline helpers such as `IB::Requests::requestMarketData`, `IB::Requests::getContractDetails`, and `IB::Request::getOptionChain` validate inputs, register promises, and forward the appropriate API calls so higher-level code can await strongly-typed results.【F:include/request/market_data/MarketDataRequests.h†L11-L50】【F:include/request/contracts/ContractDetails.h†L17-L45】【F:include/request/options/OptionChain.h†L12-L49】
- **Contract factories** – Convenience builders in `IB::Contracts` simplify instantiating stock and option `Contract` objects with sensible defaults for exchange, currency, and multipliers.【F:include/contracts/StockContracts.h†L11-L61】

## Project layout

```
.
├── CMakeLists.txt        # Builds the static IBWrapper library and links against ibapi
├── include/              # Header-only wrappers, helpers, and data structures
└── source/               # Placeholder for translation units (currently empty)
```

The top-level CMake target exports all headers under `include/` and links against the official `ibapi` library provided by Interactive Brokers.【F:CMakeLists.txt†L1-L18】

## Getting started

1. **Install prerequisites**
   - Interactive Brokers C++ API SDK (provides `EClientSocket`, `ibapi`, and headers).
   - CMake 3.14 or newer and a C++20-compatible compiler.【F:CMakeLists.txt†L1-L18】
2. **Configure & build**

   ```bash
   cmake -S . -B build
   cmake --build build
   ```

   This produces the static library `libIBWrapper.a` in the build tree that you can link into your trading applications.

3. **Link and use**
   - Include the headers you need, derive from `IBWrapperBase`, and call `connect()` to start the client session.【F:include/wrappers/IBWrapperBase.h†L52-L123】
   - Issue helper requests (e.g., `IB::Requests::getContractDetails`) and wait on their futures or extend the wrapper callbacks to pipe data into your own synchronization primitives.【F:include/request/contracts/ContractDetails.h†L17-L45】

## Extending the wrapper

You can derive custom wrappers from `IBWrapperBase` to add domain-specific callbacks, caching, or request orchestration. The existing promise maps in `IBWrapperBase` demonstrate how to bridge asynchronous IBKR responses into future-based workflows; follow the same approach to layer on additional request/response pairs.【F:include/wrappers/IBWrapperBase.h†L36-L235】

## License

This project is distributed under the MIT License. See [LICENSE](LICENSE) for details.

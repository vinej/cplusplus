# qt-finance — learning C++ with Qt + TA-Lib + Qt Charts

A small desktop app that fetches daily OHLCV bars from Yahoo Finance, plots a
candlestick chart with Qt Charts, and overlays TA-Lib indicators (SMA/EMA/RSI).

The codebase is intentionally tiny so you can read every file end-to-end.

## Layout

```
.
├── CMakeLists.txt            # build script
├── CMakePresets.json         # Visual Studio + Ninja presets
├── vcpkg.json                # dependency manifest (qt + ta-lib + json)
├── cmake/FindTALib.cmake     # locates ta-lib (vcpkg port has no CMake config)
└── src/
    ├── main.cpp              # QApplication entrypoint
    ├── MainWindow.{h,cpp}    # toolbar + chart, wires everything together
    ├── YahooFinanceClient.*  # async HTTP fetch, JSON → Candle
    ├── CandleChart.*         # QCandlestickSeries + overlays
    ├── Indicators.*          # TA-Lib wrappers (SMA/EMA/RSI)
    └── Candle.h              # OHLCV struct
```

## One-time setup (Windows)

You need: **Visual Studio 2022** (Desktop C++ workload), **Git**, **CMake ≥ 3.21**.

### 1. Install vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg C:\dev\vcpkg
C:\dev\vcpkg\bootstrap-vcpkg.bat
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\dev\vcpkg", "User")
```

Open a fresh terminal so `$env:VCPKG_ROOT` is set.

### 2. Build

vcpkg runs in *manifest mode* — when CMake configures, it reads `vcpkg.json`
and installs Qt6 + Qt Charts + TA-Lib + nlohmann-json into `vcpkg_installed/`.
The first build takes 30–60 minutes (Qt is large). Subsequent builds are fast.

```powershell
cmake --preset vs
cmake --build --preset vs
```

The executable lands in `build\vs\Debug\qt_finance.exe`. Qt DLLs are copied
next to it automatically by `windeployqt` in a post-build step.

### Alternative: Ninja

If you prefer Ninja (faster incremental builds, used by VS Code's CMake Tools):

```powershell
cmake --preset ninja-debug
cmake --build --preset ninja-debug
```

## Run

```powershell
.\build\vs\Debug\qt_finance.exe
```

Type a symbol (e.g. `AAPL`, `MSFT`, `^GSPC`), pick a range, hit **Fetch**.
Choose an indicator from the dropdown to overlay it on the chart.

## What to read first

If you're new to Qt, read in this order:

1. [src/main.cpp](src/main.cpp) — minimal `QApplication` setup
2. [src/Candle.h](src/Candle.h) — the data model
3. [src/MainWindow.cpp](src/MainWindow.cpp) — see how widgets, layouts, and
   signals/slots fit together
4. [src/YahooFinanceClient.cpp](src/YahooFinanceClient.cpp) — async HTTP via
   `QNetworkAccessManager`, plus JSON parsing
5. [src/CandleChart.cpp](src/CandleChart.cpp) — Qt Charts API
6. [src/Indicators.cpp](src/Indicators.cpp) — calling TA-Lib's C API from C++

## Ideas for next steps

- Add a volume sub-chart below the price chart
- Cache fetched data to a local SQLite DB (`Qt6::Sql`)
- Add MACD, Bollinger Bands (TA-Lib has `TA_MACD`, `TA_BBANDS` — note these
  produce multiple output arrays, so the wrapper helper needs adjusting)
- Save user state (last symbol, range) with `QSettings`
- Switch off the network and load from a CSV instead — handy for offline work

Annual Return     = (end/start)^(252/n) - 1
Annual Volatility = stddev(daily_returns) × √252
Sharpe Ratio      = (annual_return - risk_free) / annual_volatility
Std Deviation     = sample stddev of daily_returns
Skewness          = (1/n) × Σ((r - μ)³) / σ³
Kurtosis (excess) = (1/n) × Σ((r - μ)⁴) / σ⁴ - 3
VaR (95%, 1d)     = 5th-percentile of sorted daily_returns
CVaR (95%, 1d)    = mean of all returns ≤ VaR
Correlation       = pairwise: cov(X,Y) / (σx × σy)
Beta              = cov(portfolio, SPY) / var(SPY)
Alpha             = annual_return - (risk_free + β × (SPY_return - risk_free))

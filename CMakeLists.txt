cmake_minimum_required(VERSION 3.21)
project(qt_finance VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt build conveniences: run moc/uic/rcc automatically on sources that need them.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Network Sql)
find_package(nlohmann_json CONFIG REQUIRED)

find_package(talib REQUIRED)

qt_standard_project_setup()

qt_add_executable(qt_finance
    resources/app.rc
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/AnalysisWidget.cpp
    src/AnalysisWidget.h
    src/SymbolPicker.cpp
    src/SymbolPicker.h
    src/SymbolSearchClient.cpp
    src/SymbolSearchClient.h
    src/SymbolSearchEdit.cpp
    src/SymbolSearchEdit.h
    src/MarketPanel.cpp
    src/MarketPanel.h
    src/PerfSincePanel.cpp
    src/PerfSincePanel.h
    src/PerfYearPanel.cpp
    src/PerfYearPanel.h
    src/PortfolioWidget.cpp
    src/PortfolioWidget.h
    src/PortfolioDb.cpp
    src/PortfolioDb.h
    src/RebalanceDialog.cpp
    src/RebalanceDialog.h
    src/RiskWidget.cpp
    src/RiskWidget.h
    src/AiConfig.cpp
    src/AiConfig.h
    src/AiChatClient.cpp
    src/AiChatClient.h
    src/AiAssistantWidget.cpp
    src/AiAssistantWidget.h
    src/YahooFinanceClient.cpp
    src/YahooFinanceClient.h
    src/CandleChart.cpp
    src/CandleChart.h
    src/RsiChart.cpp
    src/RsiChart.h
    src/VolumeChart.cpp
    src/VolumeChart.h
    src/PerformanceChart.cpp
    src/PerformanceChart.h
    src/PortfolioAnalysisWidget.cpp
    src/PortfolioAnalysisWidget.h
    src/CollapsiblePanel.cpp
    src/CollapsiblePanel.h
    src/Indicators.cpp
    src/Indicators.h
    src/Candle.h
)

target_link_libraries(qt_finance PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::Network
    Qt6::Sql
    nlohmann_json::nlohmann_json
    talib::talib
)

set_target_properties(qt_finance PROPERTIES
    WIN32_EXECUTABLE ON
)

# Copy required Qt DLLs next to the .exe so it runs straight from the build dir on Windows.
if(WIN32)
    add_custom_command(TARGET qt_finance POST_BUILD
        COMMAND Qt6::windeployqt
            --no-translations
            --no-system-d3d-compiler
            --no-compiler-runtime
            "$<TARGET_FILE:qt_finance>"
        VERBATIM
    )
    # sqlite3.dll is a transitive dependency of the qsqlite plugin but is not
    # picked up by windeployqt — copy it manually next to the executable.
    add_custom_command(TARGET qt_finance POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/sqlite3.dll"
            "$<TARGET_FILE_DIR:qt_finance>/sqlite3.dll"
    )
endif()

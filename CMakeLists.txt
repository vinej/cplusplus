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

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts Network)
find_package(nlohmann_json CONFIG REQUIRED)

find_package(talib REQUIRED)

qt_standard_project_setup()

qt_add_executable(qt_finance
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/YahooFinanceClient.cpp
    src/YahooFinanceClient.h
    src/CandleChart.cpp
    src/CandleChart.h
    src/RsiChart.cpp
    src/RsiChart.h
    src/VolumeChart.cpp
    src/VolumeChart.h
    src/Indicators.cpp
    src/Indicators.h
    src/Candle.h
)

target_link_libraries(qt_finance PRIVATE
    Qt6::Widgets
    Qt6::Charts
    Qt6::Network
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
endif()

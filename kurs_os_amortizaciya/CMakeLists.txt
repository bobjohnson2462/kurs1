cmake_minimum_required(VERSION 3.16)
project(osmod CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(oscore
    src/model.cpp
    src/money.cpp
    src/depreciation.cpp
    src/service.cpp
    src/report.cpp
    src/sqlserver_store.cpp
)
target_include_directories(oscore PUBLIC include)
target_link_libraries(oscore PUBLIC odbc32)

add_executable(osmod
    src/main.cpp
    src/ui/input.cpp
    src/ui/reports.cpp
    src/ui/menu.cpp
)
target_include_directories(osmod PRIVATE src)
target_link_libraries(osmod PRIVATE oscore)

add_executable(osmod_tests tests/test_main.cpp)
target_link_libraries(osmod_tests PRIVATE oscore)

if(MSVC)
    target_compile_options(oscore PRIVATE /W4 /utf-8)
    target_compile_options(osmod PRIVATE /W4 /utf-8)
    target_compile_options(osmod_tests PRIVATE /W4 /utf-8)
else()
    target_compile_options(oscore PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(osmod PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(osmod_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

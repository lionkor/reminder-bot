cmake_minimum_required(VERSION 3.16)
project(reminder-bot)

add_subdirectory(deps/DPP)

include(FetchContent)

FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG master
)
FetchContent_MakeAvailable(fmt)

find_package(Threads REQUIRED) # for pthread

add_executable(reminder-bot src/main.cpp)
target_include_directories(reminder-bot PRIVATE include deps/DPP/include ${FMT_INCLUDE_DIRS})
target_link_libraries(reminder-bot dpp fmt::fmt)
 

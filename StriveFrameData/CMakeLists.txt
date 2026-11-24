cmake_minimum_required(VERSION 3.18)

set(TARGET StriveFrameData)
project(${TARGET})

set(CMAKE_CXX_STANDARD 23)

set(${TARGET}_Sources
        "${CMAKE_CURRENT_SOURCE_DIR}/src/arcsys.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/bbscript.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/dllmain.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game_reader.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/game_reporter.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/osc_config.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/osc_sender.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/risc_finder.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/sigscan.cpp"
        )

add_library(${TARGET} SHARED ${${TARGET}_Sources})
target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${TARGET} PUBLIC UE4SS ws2_32)

cmake_minimum_required(VERSION 3.16)
project(ai_dating_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(ai_dating_sim
    src/main.cpp
    src/ollama_client.cpp
    src/character.cpp
    src/utils.cpp
)

target_include_directories(ai_dating_sim
    PRIVATE
        include
)

target_link_libraries(ai_dating_sim PRIVATE nlohmann_json)

if (WIN32)
    target_link_libraries(ai_dating_sim PRIVATE ws2_32)
endif()

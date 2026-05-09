cmake_minimum_required(VERSION 3.20)

project(Fellowship VERSION 1.0.0 LANGUAGES CXX)

# Require C++23 for CommonLibSSE-NG
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CommonLibSSE-NG integration
include(FetchContent)
FetchContent_Declare(
  CommonLibSSE
  GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
  GIT_TAG main
)
set(BUILD_TESTS OFF CACHE BOOL "Disable tests" FORCE)
FetchContent_MakeAvailable(CommonLibSSE)

add_library(Fellowship SHARED 
    src/main.cpp 
    src/Networking.cpp 
    src/Hooks.cpp 
    src/ActorManager.cpp
)

target_link_libraries(Fellowship PRIVATE 
    CommonLibSSE::CommonLibSSE
    unofficial::enet::enet
    Ws2_32
    Winmm
)

# Set the output name and suffix to match SKSE plugin standards
set_target_properties(Fellowship PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)

find_package(unofficial-enet CONFIG REQUIRED)
target_link_libraries(Fellowship PRIVATE unofficial::enet::enet)

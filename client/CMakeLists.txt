cmake_minimum_required(VERSION 3.20)

project(SkyrimMultiplayerMod VERSION 1.0.0 LANGUAGES CXX)

# Require C++23 for CommonLibSSE-NG
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CommonLibSSE-NG integration
find_package(CommonLibSSE REQUIRED)

add_library(SkyrimMultiplayerMod SHARED src/main.cpp src/Networking.cpp src/Hooks.cpp src/ActorManager.cpp)

target_link_libraries(SkyrimMultiplayerMod PRIVATE CommonLibSSE::CommonLibSSE)

# Set the output name and suffix to match SKSE plugin standards
set_target_properties(SkyrimMultiplayerMod PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
)

find_package(enet CONFIG REQUIRED)
target_link_libraries(SkyrimMultiplayerMod PRIVATE enet::enet)

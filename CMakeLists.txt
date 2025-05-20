cmake_minimum_required(VERSION 3.16)
project(Selkie)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Vulkan REQUIRED)

execute_process(COMMAND bash ./shaders/compile.sh )
add_executable(${PROJECT_NAME} window.cpp renderer.cpp main.cpp)

target_link_libraries(${PROJECT_NAME} glfw ${GLFW_LIBRARIES} Vulkan::Vulkan)
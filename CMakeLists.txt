cmake_minimum_required(VERSION 3.2)
project(TGRenderer C CXX)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall")

find_package(glm REQUIRED)
find_package(sdl2 REQUIRED)
find_package(Threads)

include_directories(${GLM_INCLUDE_DIRS} stb include)
file(GLOB trcore_src src/core/*.cpp)
add_library(trcore ${trcore_src})

add_executable(TGRenderer src/main.cpp src/helper/objs.cpp src/helper/window.cpp)
target_link_libraries(TGRenderer trcore ${SDL2_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(TGRenderer PUBLIC ${SDL2_INCLUDE_DIRS})

cmake_minimum_required(VERSION 3.14)
project(MTOSFC C CXX)

set(CMAKE_CXX_STANDARD 17)
#set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Og") # -O3

#add_subdirectory(minifb)
include(FetchContent)
FetchContent_Declare(
   minifb
   GIT_REPOSITORY https://github.com/emoon/minifb.git
   GIT_TAG        master
)
FetchContent_MakeAvailable(minifb)

add_executable(MTOFCS ./src/MTOSFC.c
)

target_link_libraries(MTOFCS PRIVATE minifb)

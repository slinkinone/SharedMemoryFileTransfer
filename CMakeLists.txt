cmake_minimum_required(VERSION 3.5)

project(SharedMemoryFileTransfer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(INCLUDE_PATH "${CMAKE_SOURCE_DIR}/include")

add_compile_definitions(STDOUT_TRACE)


file(GLOB HEADERS include/*.h)
file(GLOB SOURCES src/*.cpp)

find_package(Boost REQUIRED COMPONENTS system thread)

add_executable(SharedMemoryFileTransfer main.cpp ${HEADERS} ${SOURCES})

target_include_directories(SharedMemoryFileTransfer PUBLIC ${INCLUDE_PATH})
target_link_libraries(SharedMemoryFileTransfer Boost::system Boost::thread)
target_link_libraries(SharedMemoryFileTransfer ${Boost_LIBRARIES} rt pthread)

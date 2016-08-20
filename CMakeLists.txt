cmake_minimum_required(VERSION 3.4)
project(twamp_protocol)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    client.c
    server.c
    timestamp.c
    twamp.h)

add_executable(twamp_protocol ${SOURCE_FILES})
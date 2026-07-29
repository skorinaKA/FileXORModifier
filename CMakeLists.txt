cmake_minimum_required(VERSION 3.16)
project(FileXORModifier LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(FileXORModifier
    main.cpp
    mainwindow.h mainwindow.cpp
    fileprocessor.h fileprocessor.cpp
)

target_link_libraries(FileXORModifier Qt6::Widgets)
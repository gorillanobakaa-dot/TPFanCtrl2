cmake_minimum_required(VERSION 3.16)
project(TPFanControlServiceFixture LANGUAGES CXX)
add_library(TVicPort SHARED simulated_ports.cpp TVicPort.def)

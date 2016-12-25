cmake_minimum_required(VERSION 2.8)

add_executable(adir01psend adir01psend.cpp)
target_link_libraries(adir01psend adir01pcpp)
set_property(TARGET adir01psend PROPERTY CXX_STANDARD 14)
set_property(TARGET adir01psend PROPERTY CXX_STANDARD_REQUIRED on)
target_enable_warning(adir01psend)

add_executable(adir01pReceiveTest adir01pReceiveTest.cpp)
target_link_libraries(adir01pReceiveTest adir01pcpp)
set_property(TARGET adir01pReceiveTest PROPERTY CXX_STANDARD 14)
set_property(TARGET adir01pReceiveTest PROPERTY CXX_STANDARD_REQUIRED on)
target_enable_warning(adir01pReceiveTest)

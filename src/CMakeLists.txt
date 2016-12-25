cmake_minimum_required(VERSION 2.8)

add_library(adir01pcpp adir01pcpp.cpp)
set_property(TARGET adir01pcpp PROPERTY CXX_STANDARD 14)
set_property(TARGET adir01pcpp PROPERTY CXX_STANDARD_REQUIRED on)
target_include_directories(adir01pcpp PUBLIC ../include)

find_library(LIBUSB_1_LIBRARIES usb-1.0)
target_link_libraries(adir01pcpp ${LIBUSB_1_LIBRARIES})
target_enable_warning(adir01pcpp)

cmake_minimum_required(VERSION 3.5)

project(camtest)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(DIPlib 3.0 REQUIRED DIP DIPviewer)
find_package(OpenCV REQUIRED COMPONENTS core imgproc highgui videoio)
find_package(Threads)

add_executable(camtest camtest.cpp)
target_link_libraries(camtest PRIVATE DIPlib::DIP DIPlib::DIPviewer ${OpenCV_LIBS} Threads::Threads)

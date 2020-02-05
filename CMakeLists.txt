cmake_minimum_required(VERSION 3.5)

project(camtest)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(DIPlib 3.0 REQUIRED DIP DIPviewer)
find_package(OpenCV REQUIRED COMPONENTS core videoio)
find_package(PkgConfig REQUIRED)
find_package(Threads)

pkg_check_modules(V4L2 libv4l2)
pkg_check_modules(TURBOJPEG libturbojpeg)

add_executable(camtest camtest.cpp)
target_link_libraries(camtest PRIVATE DIPlib::DIP DIPlib::DIPviewer ${OpenCV_LIBS} Threads::Threads)

if (V4L2_FOUND)
  add_executable(v4ltest v4ltest.cpp)
  target_link_libraries(v4ltest PRIVATE DIPlib::DIP DIPlib::DIPviewer ${V4L2_LIBRARIES} ${TURBOJPEG_LIBRARIES} Threads::Threads)
endif()

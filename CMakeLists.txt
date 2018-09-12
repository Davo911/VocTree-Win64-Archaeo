PROJECT(voctree)
cmake_minimum_required(VERSION 3.1)

IF(CMAKE_BUILD_TYPE MATCHES Release)
    message("Release mode")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W0 -MT")
ENDIF()

IF(CMAKE_BUILD_TYPE MATCHES Debug)
    message("Debug mode")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -Wall")
ENDIF()

# In case cmake didn't find OpenCV set variable OpenCV_DIR correctly
#set(OpenCV_DIR ./OCVinstall/)
find_package(OpenCV REQUIRED)

add_subdirectory(src)

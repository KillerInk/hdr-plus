cmake_minimum_required(VERSION 2.8.11)
project(HDR_PLUS)

set(CMAKE_CXX14_EXTENSION_COMPILE_OPTION -std=c++14)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-O3 -g -Wall")

set(HALIDE_ROOT_DIR "")
set(PNG_ROOT_DIR "")
set(JPEG_ROOT_DIR "")
set(LIBRAW_ROOT "")

find_package(PNG REQUIRED)
if(" ${HALIDE_ROOT_DIR}" STREQUAL " ")
    message( FATAL_ERROR "Specify HALIDE_ROOT_DIR variable in the cmake file." )
endif()

if (ANDROID)
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_CXX_FLAGS}")
endif()

set(src_files
    src/HDRPlus.cpp
    src/align.cpp
    src/finish.cpp
    src/merge.cpp
    src/util.cpp
    )

include_directories(${HALIDE_ROOT_DIR}/include ${HALIDE_ROOT_DIR}/tools ${PNG_ROOT_DIR}/ ${JPEG_ROOT_DIR}/ ${LIBRAW_ROOT}/libraw ${LIBRAW_ROOT}/src)
link_directories(${HALIDE_ROOT_DIR}/bin ${LIBRAW_ROOT}/lib)

add_executable(hdrplus ${src_files})

target_link_libraries(hdrplus Halide PNG)

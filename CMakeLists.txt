###############################################################################
#
# Top level CMakeLists.txt file for the SYCL-RNG repo
# Hacker: Mate Ferenc Nagy-Egri
# Last modified: 2018.08.01. (yyyy.mm.dd.)
#
###############################################################################

#########################
#                       #
#         CMake         #
#                       #
#########################

# The minimum version of 'cmake' necessary to build this project
cmake_minimum_required (VERSION 3.2.2)

# The project name and version, release numbers
project (LandCounts LANGUAGES CXX
                    VERSION 0.0.1)

# Behavioural options for the project
set (CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/SYCL-PRNG/cmake/Modules)

add_executable (LandCounts LandCounts/main.cpp)

target_include_directories (LandCounts PRIVATE ${PROJECT_SOURCE_DIR}/SYCL-PRNG/include
                                               ${PROJECT_SOURCE_DIR}/extern/concurrentqueue)

set_target_properties (LandCounts PROPERTIES CXX_STANDARD 20
                                             CXX_STANDARD_REQUIRED ON
                                             CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options (LandCounts PRIVATE -march=native)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options (LandCounts PRIVATE /arch:AVX2)
endif()

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)
target_link_libraries (LandCounts PRIVATE Threads::Threads)
# target_compile_options(LandCounts PRIVATE -fsycl -fsycl-targets=nvptx64-nvidia-cuda-sycldevice -fsycl-device-lib=all --cuda-path=/opt/cuda)

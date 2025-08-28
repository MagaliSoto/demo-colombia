cmake_minimum_required(VERSION 3.19)
project(multicamera_tracking VERSION 1.0.0 LANGUAGES C CXX CUDA)

# if(POLICY CMP0146)
#   cmake_policy(SET CMP0146 OLD)
# endif()

# Set the installation directory
if(NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/app" CACHE PATH "Installation directory" FORCE)
endif()

# Add the cmake directory to the module path
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# <---------- set c++ standard ------------->
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# add_compile_options(-std=c++2a)

# It is important to include or find PkgConfig first, then we have 2 functions:
# - pkg_search_module, which checks for the package and uses the first available.
# - pkg_check_modules, which check for all the corresponding packages.
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(gflags REQUIRED)
find_package(GStreamer REQUIRED)
find_package(DeepstreamAppCommon REQUIRED)
find_package(NVDS REQUIRED)
find_package(CUDA REQUIRED)
find_package(JsonGlib REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Mosquitto REQUIRED)
find_package(OpenCV 4 REQUIRED COMPONENTS 
  core 
  highgui 
  videoio 
  imgproc 
  imgcodecs
  tracking  # <- for Kalman Filter
  # The lines below are for using CUDA
  cudaimgproc
  cudaoptflow
  cudaarithm
)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
# <------------ add hiredis dependency --------------->
find_path(HIREDIS_HEADER hiredis)
# target_include_directories(target PUBLIC ${HIREDIS_HEADER})

find_library(HIREDIS_LIB hiredis)
# target_link_libraries(target ${HIREDIS_LIB})

# <------------ add redis-plus-plus dependency -------------->
# NOTE: this should be *sw* NOT *redis++*
find_path(REDIS_PLUS_PLUS_HEADER sw)
# target_include_directories(target PUBLIC ${REDIS_PLUS_PLUS_HEADER})

find_library(REDIS_PLUS_PLUS_LIB redis++)
# target_link_libraries(target ${REDIS_PLUS_PLUS_LIB})


include_directories(
  include
  ${HIREDIS_HEADER}
  ${REDIS_PLUS_PLUS_HEADER}
  # libb64/include
  ${GSTREAMER_INCLUDE_DIRS}
  ${GSTREAMER_RTSPSERVER_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${GTK3_INCLUDE_DIRS}
  ${JsonGlib_INCLUDE_DIRS}
  ${DEEPSTREAM_APP_COMMON_INCLUDE_DIRS}
  ${NVDS_INCLUDE_DIRS}
  ${CUDA_INCLUDE_DIRS}
  ${YAML_CPP_INCLUDE_DIR}
  ${GFLAGS_INCLUDE_DIRS}
  ${MOSQUITTO_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

# add_subdirectory(libb64)

# Building:
# =========
add_executable(perception src/perception.cpp src/Pipeline.cpp ${DEEPSTREAM_APP_COMMON_SRCS})

# # Define the buffer size for libb64 (MANDATORY!)
# # A larger size should be faster, but takes more runtime memory
# # Example sizes: 4096, or 65536 or 16777216
# target_compile_definitions(perception PRIVATE BUFFERSIZE=16777216)

target_link_libraries(perception ${HIREDIS_LIB} ${REDIS_PLUS_PLUS_LIB} ${OpenCV_LIBRARIES} ${GSTREAMER_LIBRARIES} ${GSTREAMER_RTSPSERVER_LIBRARIES} ${GLIB_LIBRARIES} ${GTK3_LIBRARIES} ${JsonGlib_LIBRARIES} ${NVDS_LIBRARIES} ${CUDA_LIBRARIES} ${CMAKE_DL_LIBS} ${YAML_CPP_LIBRARIES} ${GFLAGS_LIBRARIES} ${MOSQUITTO_LIBRARIES} Threads::Threads)

# Installation:
# =============
# Copy built files to the installation directory
install(TARGETS perception
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
# install(FILES scripts/functions.bash
#   DESTINATION ${CMAKE_INSTALL_PREFIX}/scripts)

# # Install config files
# install(DIRECTORY cfg/
#   DESTINATION ${CMAKE_INSTALL_PREFIX}/cfg)

# # Install model files
# install(DIRECTORY models/
#   DESTINATION ${CMAKE_INSTALL_PREFIX}/models)

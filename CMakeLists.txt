#Ubuntu 20.04 uses 3.16.3
#Ubuntu 22.04 uses 3.22.1
#Arch currently on 3.26.3
#Cygwin currently using 3.23.2 (or newer)
cmake_minimum_required(VERSION 3.10.2)
project(brute)
#set(CMAKE_C_STANDARD 99)

include(FindOpenMP)
find_package(OpenMP)
if(OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
else(OPENMP_FOUND)
    message("ERROR: OpenMP could not be found.")
endif(OPENMP_FOUND)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

#use cmake option -DCOLORS=OFF to disable color output
option(COLORS
    "Build with Colors Enabled" ON)
if (COLORS)
    add_definitions(-DPRETTY_COLORS)
endif ()

find_package(LibSndFile REQUIRED)

include_directories(SYSTEM ${LIBSNDFILE_INCLUDE_DIR})
set(LIBS ${LIBSNDFILE_LIBRARY})

FILE(GLOB SRCS src/*.c src/*.cpp)
FILE(GLOB HEADERS include/*.h include/*.hpp)

include_directories("${PROJECT_SOURCE_DIR}/include")

ADD_EXECUTABLE(brute ${SRCS} ${HEADERS})
TARGET_LINK_LIBRARIES(brute ${LIBS})

install(TARGETS brute DESTINATION ${CMAKE_INSTALL_BINDIR})

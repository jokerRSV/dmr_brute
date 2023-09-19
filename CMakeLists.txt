#Ubuntu 20.04 uses 3.16.3
#Ubuntu 22.04 uses 3.22.1
#Arch currently on 3.26.3
#Cygwin currently using 3.23.2 (or newer)
cmake_minimum_required(VERSION 3.10.2)
project(dsd-fme)
set(CMAKE_C_STANDARD 99)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

#use cmake option -DCOLORS=OFF to disable color output
option(COLORS
    "Build with Colors Enabled" ON)
if (COLORS)
    add_definitions(-DPRETTY_COLORS)
endif ()

find_package(LibSndFile REQUIRED)
#find_package(MBE REQUIRED)
#find_package(ITPP REQUIRED)
#find_package(Curses REQUIRED)
find_package(PulseAudio REQUIRED)

include_directories(SYSTEM ${LIBSNDFILE_INCLUDE_DIR} ${PULSEAUDIO_INCLUDE_DIRS})
set(LIBS ${LIBSNDFILE_LIBRARY} ${PULSEAUDIO_SIMPLE_LIBRARY})

FILE(GLOB SRCS src/*.c src/*.cpp)
FILE(GLOB HEADERS include/*.h include/*.hpp)

include_directories("${PROJECT_SOURCE_DIR}/include")

ADD_EXECUTABLE(dsd-fme ${SRCS} ${HEADERS})
TARGET_LINK_LIBRARIES(dsd-fme ${LIBS})

install(TARGETS dsd-fme DESTINATION ${CMAKE_INSTALL_BINDIR})

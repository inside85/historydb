project(historydb)
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
cmake_policy (VERSION 2.6.0)
add_definitions(-std=c++0x -W -Wall)

FILE (READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+)" HISTORYDB_VERSION_ABI "${DEBFULLVERSION}")
STRING (REGEX MATCH "([0-9]+\\.[0-9]+$)" HISTORYDB_VERSION_MINOR "${DEBFULLVERSION}")

INCLUDE(cmake/locate_library.cmake)
LOCATE_LIBRARY(ELLIPTICS_CLIENT "elliptics/interface.h" "elliptics_client")
LOCATE_LIBRARY(ELLIPTICS_CPP "elliptics/cppdef.h" "elliptics_cpp")
LOCATE_LIBRARY(MSGPACK "msgpack/unpack.hpp" "msgpack")

find_package(Boost REQUIRED COMPONENTS system thread)

INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${ELLIPTICS_CLIENT_INCLUDE_DIRS}
	${MSGPACK_INCLUDE_DIRS}
)

LINK_DIRECTORIES(
	${ELLIPTICS_CLIENT_LIBRARY_DIRS}
	${MSGPACK_LIBRARY_DIRS}
)

add_subdirectory(src/app)
add_subdirectory(src/lib)
add_subdirectory(src/fastcgi)
add_subdirectory(src/thevoid)

install(FILES
	include/historydb/provider.h
	DESTINATION include/historydb/
)

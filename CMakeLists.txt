cmake_minimum_required(VERSION 2.8)
project(stpwrap2)

set(EXEC_NAME ${CMAKE_PROJECT_NAME})

find_package(FLEX)

if(NOT FLEX_FOUND )
	message(FATAL_ERROR "Couldn't find flex!")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

FLEX_TARGET(InputLex il.y ${CMAKE_CURRENT_BINARY_DIR}/inputlexer.cpp)
FLEX_TARGET(OutputLex ol.y ${CMAKE_CURRENT_BINARY_DIR}/outputlexer.cpp)

if(NOT FLEX_InputLex_DEFINED)
	message(FATAL_ERROR "Flex cmake macro failed!")
endif()

if(NOT FLEX_OutputLex_DEFINED)
	message(FATAL_ERROR "Flex cmake macro failed!")
endif()

add_executable(${EXEC_NAME}
	       main.cpp
	       Array.cpp
	       ${FLEX_InputLex_OUTPUTS}
	       ${FLEX_OutputLex_OUTPUTS}
	       )

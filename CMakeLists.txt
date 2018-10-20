cmake_minimum_required (VERSION 2.8)
project (PW3 C)

set(CMAKE_C_FLAGS "-Wall")

# http://stackoverflow.com/questions/10555706/
macro (add_executable _name)
    # invoke built-in add_executable
    _add_executable(${ARGV})
    if (TARGET ${_name})
        target_link_libraries(${_name} rt m)
    endif()
endmacro()

add_library(automaton automaton.c)
add_library(messageconsts messages.c)
add_library(statearray statearray.c)
add_library(stats stats.c)
add_library(utils utils.c)
add_executable(run run.c mylimits.h mylimits.h statearray.c statearray.h automaton.c automaton.h utils.c utils.h messages.c messages.h)
add_executable(validator validator.c mytypes.h statearray.c statearray.h automaton.c automaton.h stats.c stats.h messages.c messages.h utils.c utils.h)
add_executable(tester tester.c mytypes.h mylimits.h messages.c messages.h stats.c stats.h utils.c utils.h)

install(TARGETS DESTINATION .)


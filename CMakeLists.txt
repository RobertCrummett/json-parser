cmake_minimum_required(VERSION 3.10)
project(MyApp C)
# set(CMAKE_C_STANDARD 89)

if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(/W4 /WX)
else()
	add_compile_options(-std=c89 -pedantic -Wall -Wextra -Werror)
endif()

add_executable(MyApp main.c json.c)

cmake_minimum_required(VERSION 3.10)
project(MyApp C)

# Add warnings
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	add_compile_options(/W3 /WX)
else()
	add_compile_options(-std=c89 -pedantic -Wall -Wextra -Werror)
endif()

add_executable(MyApp main.c json.c)

# This definition is included becuase Visual Studio likes to tell me fread and strerror are unsafe functions
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
	target_compile_definitions(MyApp PRIVATE -D_CRT_SECURE_NO_WARNINGS)
endif()
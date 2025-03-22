cmake_minimum_required(VERSION 3.10)
project(MyApp C)

# Create the executable
add_executable(MyApp main.c json.c)
target_compile_options(MyApp PRIVATE -Wall -Wextra -Wpedantic -ggdb)
target_include_directories(MyApp PRIVATE include)
target_link_libraries(MyApp m)

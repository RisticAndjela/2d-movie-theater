cmake_minimum_required(VERSION 3.10)
project(2d-movie-theater LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
add_executable(movie
    src/main.cpp
    include/Shader.cpp
)
target_include_directories(movie PRIVATE include)
target_link_libraries(movie PRIVATE OpenGL::GL glfw)

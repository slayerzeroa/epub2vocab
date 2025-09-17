cmake_minimum_required(VERSION 3.16)
project(epub_reader CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# vcpkg 연동 시 자동으로 find_package 가능
find_package(libzip REQUIRED)
find_package(pugixml REQUIRED)

add_executable(epub_reader src/main.cpp)
# target_link_libraries(epub_reader PRIVATE zip pugixml)

target_link_libraries(epub_reader PRIVATE libzip::zip pugixml::pugixml)
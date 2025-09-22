cmake_minimum_required(VERSION 3.20)
project(epub2vocab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libzip CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

add_library(epub_reader
    src/functions/epub_reader/src/epub_reader.cpp
)

target_include_directories(epub_reader
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src/functions/epub_reader/src
)

target_link_libraries(epub_reader
    PUBLIC libzip::zip pugixml::pugixml
)

add_executable(epub2vocab
    src/main.cpp
)

target_link_libraries(epub2vocab PRIVATE epub_reader)

# (MinGW용) 콘솔 서브시스템
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_link_options(epub2vocab PRIVATE "-mconsole")
endif()

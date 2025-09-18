add_library(epub_reader
  functions/epub_reader/src/epub_reader.cpp
)
target_include_directories(epub_reader PUBLIC functions/epub_reader/src)

add_executable(epub2vocab main.cpp)
target_link_libraries(epub2vocab PRIVATE epub_reader zip pugixml)
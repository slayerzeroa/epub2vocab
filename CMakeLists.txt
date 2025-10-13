cmake_minimum_required(VERSION 3.20)
project(epub2vocab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libzip CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)

add_library(epub_reader
    src/functions/epub_reader/src/epub_reader.cpp
)

set(WORD_EXTRACTOR_SOURCES
  src/functions/word_extractor/src/word_extractor.cpp
  src/functions/word_extractor/src/word_extractor.hpp
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
    ${WORD_EXTRACTOR_SOURCES}
)

target_link_libraries(epub2vocab PRIVATE epub_reader)

add_subdirectory(src/functions/py_runner)

target_link_libraries(epub2vocab PRIVATE
  py_runner
)


# words.txt / stopwords.txt 복사 (실행파일 옆으로)
set(WORDS_TXT     "${CMAKE_CURRENT_SOURCE_DIR}/src/functions/word_extractor/words.txt")
set(STOPWORDS_TXT "${CMAKE_CURRENT_SOURCE_DIR}/src/functions/word_extractor/stopwords.txt")

add_custom_command(TARGET epub2vocab POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:epub2vocab>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${WORDS_TXT}"     "$<TARGET_FILE_DIR:epub2vocab>/words.txt"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${STOPWORDS_TXT}" "$<TARGET_FILE_DIR:epub2vocab>/stopwords.txt"
  VERBATIM
  COMMENT "Copying words/stopwords next to epub2vocab.exe"
)

# (MinGW용) 콘솔 서브시스템
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_link_options(epub2vocab PRIVATE "-mconsole")
endif()
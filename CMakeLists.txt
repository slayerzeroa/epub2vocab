cmake_minimum_required(VERSION 3.20)
project(epub2vocab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libzip CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

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

set(WORD_EXTRACTOR_SOURCES
  src/functions/word_extractor/src/word_extractor.cpp
  src/functions/word_extractor/src/word_extractor.hpp
)


add_executable(epub2vocab
    src/main.cpp
    ${WORD_EXTRACTOR_SOURCES}
)


target_link_libraries(epub2vocab 
  PRIVATE
    epub_reader
    py_runner
    connect_dictionary
    send_telegram
)

add_subdirectory(src/functions/py_runner)

target_link_libraries(epub2vocab PRIVATE
  py_runner
)

add_library(connect_dictionary
    src/functions/connect_dictionary/src/connect_dictionary.cpp
    src/functions/connect_dictionary/src/connect_dictionary.hpp
)

target_link_libraries(connect_dictionary
  PUBLIC
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

target_include_directories(connect_dictionary
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src/functions/connect_dictionary/src
)


add_library(send_telegram
    src/functions/send_telegram/src/send_telegram.cpp
    src/functions/send_telegram/src/send_telegram.hpp
)

target_link_libraries(send_telegram
  PUBLIC
    CURL::libcurl
)

target_include_directories(send_telegram
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/src/functions/send_telegram/src
)


# words.txt / stopwords.txt 복사 (실행파일 옆으로)
set(WORDS_TXT     "${CMAKE_CURRENT_SOURCE_DIR}/src/functions/word_extractor/words.txt")
set(STOPWORDS_TXT "${CMAKE_CURRENT_SOURCE_DIR}/src/functions/word_extractor/stopwords.txt")
set(DOTENV          "${CMAKE_CURRENT_SOURCE_DIR}/src/.env")

add_custom_command(TARGET epub2vocab POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:epub2vocab>"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${WORDS_TXT}"     "$<TARGET_FILE_DIR:epub2vocab>/words.txt"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${STOPWORDS_TXT}" "$<TARGET_FILE_DIR:epub2vocab>/stopwords.txt"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${DOTENV}"     "$<TARGET_FILE_DIR:epub2vocab>/.env"
  VERBATIM
  COMMENT "Copying words/stopwords next to epub2vocab.exe"
)

# (MinGW용) 콘솔 서브시스템
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_link_options(epub2vocab PRIVATE "-mconsole")
endif()
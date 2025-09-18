#include "functions/epub_reader/src/epub_reader.hpp"
#include <iostream>
#include <fstream>

int main() {
    try {
        std::string text = extract_epub_text("book.epub");
        std::cout << "텍스트 길이: " << text.size() << " chars\n";

        // 원하면 파일로 저장
        std::ofstream("book_text.txt") << text;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
    }
}

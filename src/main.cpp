#include "functions/epub_reader/src/epub_reader.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: epub2vocab <file.epub>\n";
        return 1;
    }
    const char* path = argv[1];
    try {
        std::string text = extract_epub_text(path);
        std::cout << "텍스트 길이: " << text.size() << " chars\n";
        std::ofstream("book_text.txt") << text;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
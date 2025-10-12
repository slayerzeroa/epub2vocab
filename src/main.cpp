#include "functions/epub_reader/src/epub_reader.hpp"
#include "functions/word_extractor/src/word_extractor.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: epub2vocab <sample/sample.epub>\n";
        return 1;
    }
    const char* path = argv[1];
    try {
        std::string text = extract_epub_text(path);
        std::cout << "text size: " << text.size() << " chars\n";
        std::ofstream("book_text.txt") << text;
        word_extractor_main(text);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
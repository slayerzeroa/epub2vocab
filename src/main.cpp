#include "functions/epub_reader/src/epub_reader.hpp"
#include "functions/word_extractor/src/word_extractor.hpp"
#include "py_runner/py_runner.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
  #include <windows.h>
#endif

namespace fs = std::filesystem;

// exe가 있는 디렉토리
static fs::path exe_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return n ? fs::path(buf).parent_path() : fs::current_path();
#else
    return fs::current_path();
#endif
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: epub2vocab <sample/sample.epub>\n";
        return 1;
    }
    const char* path = argv[1];

    try {
        // exe 폴더 경로
        const fs::path exeDir = exe_dir();

        // EPUB → 텍스트
        std::string text = extract_epub_text(path);
        std::cout << "text size: " << text.size() << " chars\n";

        // exe 옆에 저장
        const fs::path bookTextPath = exeDir / "book_text.txt";
        {
            std::ofstream ofs(bookTextPath, std::ios::binary);
            if (!ofs) throw std::runtime_error("failed to create: " + bookTextPath.string());
            ofs << text;
        }
        std::cout << "[info] Saved full text to " << bookTextPath.string() << "\n";

        // 단어 추출 (내부에서 exe 디렉토리 찾는 코드 없다면 필요시 넘겨도 OK)
        word_extractor_main(text);

        std::cout << "[info] Saved unique words to vocab.txt\n";

        // (선택) 파이썬 레마타이저 실행: vocab.txt → vocab_lemma.txt
        run_lemmatizer(exeDir);

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
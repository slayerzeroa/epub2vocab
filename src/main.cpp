#include "functions/epub_reader/src/epub_reader.hpp"
#include "functions/word_extractor/src/word_extractor.hpp"
#include "functions/connect_dictionary/src/connect_dictionary.hpp"
#include "py_runner/py_runner.hpp"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <array>
#include <vector>
#include <numeric>   // iota
#include <algorithm> // sample
#include <random>

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

    // argv[1] : epub 파일 경로
    // argv[2] : (선택) 추출할 단어 개수 (기본 5개)

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

        // (선택) 사전 API 연결: vocab_lemma.txt → definition.txt
        // vocab_lemma 읽기
        const fs::path vocabLemmaPath = exeDir / "vocab_lemma.txt";

        // vocab_lemma에서 단어 하나 랜덤 뽑아서 connect_dictionary 실행
        if (fs::exists(vocabLemmaPath)) {
            std::ifstream ifs(vocabLemmaPath);
            std::string line;

            // vocab_lemma 단어 개수 확인 (vocab lemma 첫번째 줄은 단어 개수)
            int wordsLength = 0;
            if (std::getline(ifs, line)) {
                try {
                    wordsLength = std::stoi(line);
                } catch (...) {
                    wordsLength = 0;
                }
            }

            
            // target 단어 개수
            std::array<int, 5> targetWords;

            std::vector<int> idx(wordsLength - 1);
            std::iota(idx.begin(), idx.end(), 1);  // 1..(wordsLength-1)

            std::random_device rd;
            std::mt19937 gen(rd());
            std::shuffle(idx.begin(), idx.end(), gen);

            for (int i = 0; i < 5; ++i) targetWords[i] = idx[i];
            
            int count = 0;

            std::string wholeLines;
            for (int target : targetWords) {
                ifs.clear();
                ifs.seekg(0, std::ios::beg);
                count = 0;
                while (std::getline(ifs, line)) {
                    if (!line.empty()) {
                        if (count == target) {
                            std::string response = print_definition(line);
                            wholeLines += response + "\n";
                            wholeLines += "----------------------\n";
                            std::cout << "[info] Saved definitions to definition.txt\n";
                            break;
                        }
                        count++;
                    }
                }
            }
            save_to_file((exeDir / "definition.txt").string(), wholeLines);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
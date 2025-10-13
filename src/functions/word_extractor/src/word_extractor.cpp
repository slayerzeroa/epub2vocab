// #include <cctype>
// #include <string>
// #include <set>
// #include <unordered_set>
// #include <fstream>
// #include <iostream>

// // ========== 정규화 유틸 (그대로) ==========
// static void normalize_apostrophes_and_dashes(std::string& s) {
//     std::string out; out.reserve(s.size());
//     const size_t n = s.size();
//     for (size_t i = 0; i < n; ++i) {
//         unsigned char c = (unsigned char)s[i];
//         if (i + 2 < n && c == 0xE2 && (unsigned char)s[i+1] == 0x80 && (unsigned char)s[i+2] == 0x99)
//         { out.push_back('\''); i += 2; continue; }                 // UTF-8 ’ -> '
//         if (i + 2 < n && c == 0xE2 && (unsigned char)s[i+1] == 0x80 && (unsigned char)s[i+2] == 0x93)
//         { out.push_back('-');  i += 2; continue; }                 // UTF-8 – -> -
//         if (c == 0x92) { out.push_back('\''); continue; }          // CP1252 ’ -> '
//         if (i + 1 < n && c == 0xA1) {                              // CP949 ‘/’ -> '
//             unsigned char c2 = (unsigned char)s[i+1];
//             if (c2 == 0xAE || c2 == 0xAF) { out.push_back('\''); i += 1; continue; }
//         }
//         if (i + 1 < n && c == 0x3F && (unsigned char)s[i+1] == 0x99)
//         { out.push_back('\''); i += 1; continue; }                  // 깨짐 '? 0x99' 패턴
//         out.push_back((char)c);
//     }
//     s.swap(out);
// }

// static inline bool is_sentence_terminator(unsigned char c) {
//     return c=='.' || c=='!' || c=='?';
// }

// // ========== 공통 로더 ==========
// static std::unordered_set<std::string> load_wordlist(const char* path) {
//     std::unordered_set<std::string> set;
//     std::ifstream fin(path, std::ios::binary);
//     if (!fin) {
//         std::cerr << "[warn] cannot open " << path << " — set is empty.\n";
//         return set;
//     }
//     std::string line;
//     while (std::getline(fin, line)) {
//         // trim
//         while (!line.empty() && (line.back()=='\r' || std::isspace((unsigned char)line.back()))) line.pop_back();
//         size_t p = 0; while (p < line.size() && std::isspace((unsigned char)line[p])) ++p;
//         if (p) line.erase(0, p);
//         if (line.empty()) continue;

//         // 정규화 + 소문자화 후 삽입
//         normalize_apostrophes_and_dashes(line);
//         for (auto& ch : line) ch = (char)std::tolower((unsigned char)ch);
//         set.insert(std::move(line));
//     }
//     return set;
// }

// // ========== 전역 캐시 ==========
// static const std::unordered_set<std::string>& DICT() {
//     static const std::unordered_set<std::string> dict = load_wordlist("words.txt");
//     return dict;
// }
// static const std::unordered_set<std::string>& STOP() {
//     static const std::unordered_set<std::string> stop = load_wordlist("stopwords.txt");
//     return stop;
// }

// // ========== 요구사항 반영: proper 제외 + 한 글자 제외 + words 화이트리스트 + stopwords 블랙리스트 ==========
// static std::set<std::string> unique_words(std::string text) {
//     normalize_apostrophes_and_dashes(text);

//     auto is_alpha = [](unsigned char x){ return std::isalpha(x) != 0; };
//     auto to_lower = [](unsigned char x){ return (char)std::tolower(x); };

//     std::set<std::string> words;
//     std::string cur_norm, cur_orig;
//     const size_t n = text.size();

//     bool in_token = false;
//     bool at_sentence_start = true;
//     bool token_started_at_sentence_start = false;

//     auto flush_token = [&](){
//         if (!in_token) return;

//         // (1) 한 글자 제외
//         if (cur_norm.size() <= 1) { cur_norm.clear(); cur_orig.clear(); in_token=false; return; }

//         // (2) 고유명사(문장 중간 TitleCase) 제외 + ALL-CAPS 제외
//         bool has_lower=false, first_is_upper=false, all_caps=true;
//         for (size_t i=0;i<cur_orig.size();++i){
//             unsigned char c = (unsigned char)cur_orig[i];
//             if (std::isalpha(c)) {
//                 if (!std::isupper(c)) all_caps = false;
//                 if (i==0) first_is_upper = std::isupper(c)!=0;
//                 if (std::islower(c)) has_lower = true;
//             }
//         }
//         bool looks_titlecase = first_is_upper && has_lower;            // e.g., Google, Seoul
//         bool is_proper_like  = looks_titlecase && !token_started_at_sentence_start;

//         // (3) words.txt 화이트리스트 + stopwords.txt 블랙리스트
//         const auto& dict = DICT();
//         const auto& stop = STOP();
//         bool in_dict = (dict.find(cur_norm) != dict.end());
//         bool in_stop = (stop.find(cur_norm) != stop.end());

//         // 최종 포함 조건: (proper 아님) AND (ALL-CAPS 아님) AND (사전에 있음) AND (스톱워드가 아님)
//         if (!is_proper_like && !all_caps && in_dict && !in_stop) {
//             words.insert(cur_norm);
//         }

//         cur_norm.clear(); cur_orig.clear(); in_token=false;
//     };

//     for (size_t i = 0; i < n; ++i) {
//         unsigned char c = (unsigned char)text[i];
//         if (c == 0x1A) continue; // Ctrl+Z 무시

//         if (is_alpha(c)) {
//             if (!in_token) { in_token=true; token_started_at_sentence_start = at_sentence_start; }
//             cur_orig.push_back((char)c);
//             cur_norm.push_back(to_lower(c));
//             at_sentence_start = false;
//             continue;
//         }

//         // 내부 연결자 포함
//         if ((c == '-' || c == '\'') &&
//             in_token && i + 1 < n && is_alpha((unsigned char)text[i+1]))
//         {
//             cur_orig.push_back((char)c);
//             cur_norm.push_back((char)c);
//             continue;
//         }

//         // 토큰 종료
//         flush_token();

//         // 문장 시작 판정 갱신
//         if (c=='.' || c=='!' || c=='?') at_sentence_start = true;
//         else if (!std::isspace(c))      at_sentence_start = false;
//     }
//     flush_token();

//     return words;
// }



// static void dump_bytes(const std::string& s){
//     std::cout << "bytes:";
//     for(unsigned char b: s) std::cout << " " << std::hex << std::uppercase << (int)b;
//     std::cout << std::dec << "\n";
// }


// int word_extractor_main(std::string line) {
//     std::string text;
//     while (std::getline(std::cin, line)) {
//         text += line + "\n";
//     }

//     // dump_bytes(text);


//     auto words = unique_words(text);
//     std::cout << "Unique words found (" << words.size() << "):\n";
//     for (const auto& w : words) {
//         std::cout << w << "\n";
//     }
//     return 0;
// }


#include <cctype>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <functional>
#include <chrono>
#include <iomanip>


#include <filesystem>
#ifdef _WIN32
  #include <windows.h>
#endif
static std::filesystem::path exe_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return (n ? std::filesystem::path(buf).parent_path() : std::filesystem::current_path());
#else
    return std::filesystem::current_path(); // (필요 시 /proc/self/exe 등으로 보강)
#endif
}

// 진행률 콜백 타입: on_progress(processed_bytes, total_bytes)
using ProgressFn = std::function<void(size_t,size_t)>;

// 빠른 ASCII 판정/소문자화 (유니코드 복잡성은 기존 규칙 한정)
inline bool ascii_is_alpha(unsigned char c) {
    return (c|32) >= 'a' && (c|32) <= 'z';
}
inline char ascii_to_lower(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return char(c + ('a' - 'A'));
    return char(c);
}

// words.txt / stopwords.txt 로더 (기존과 동일)
static std::unordered_set<std::string> load_wordlist(const char* path) {
    std::unordered_set<std::string> set;
    std::ifstream fin(path, std::ios::binary);
    if (!fin) return set;
    std::string line;
    while (std::getline(fin, line)) {
        // trim
        while (!line.empty() && (line.back()=='\r' || std::isspace((unsigned char)line.back()))) line.pop_back();
        size_t p = 0; while (p < line.size() && std::isspace((unsigned char)line[p])) ++p;
        if (p) line.erase(0, p);
        if (line.empty()) continue;

        // 사전 항목은 간단 정규화: ’/‘/– 등은 ASCII로
        // (빈번한 케이스만 처리)
        std::string out; out.reserve(line.size());
        for (size_t i=0;i<line.size();++i) {
            unsigned char c = (unsigned char)line[i];
            // UTF-8 ’ = E2 80 99
            if (i+2<line.size() && c==0xE2
                && (unsigned char)line[i+1]==0x80
                && (unsigned char)line[i+2]==0x99) { out.push_back('\''); i+=2; continue; }
            // UTF-8 – = E2 80 93
            if (i+2<line.size() && c==0xE2
                && (unsigned char)line[i+1]==0x80
                && (unsigned char)line[i+2]==0x93) { out.push_back('-'); i+=2; continue; }
            // CP1252 ’
            if (c==0x92) { out.push_back('\''); continue; }
            // CP949 ‘/’
            if (i+1<line.size() && c==0xA1) {
                unsigned char c2=(unsigned char)line[i+1];
                if (c2==0xAE || c2==0xAF) { out.push_back('\''); i+=1; continue; }
            }
            out.push_back(ascii_to_lower(c));
        }
        set.insert(std::move(out));
    }
    return set;
}


static std::filesystem::path locate_file(const char* name) {
    namespace fs = std::filesystem;
    const fs::path cand1 = fs::current_path() / name; // CWD
    if (fs::exists(cand1)) return cand1;
    const fs::path cand2 = exe_dir() / name;          // exe 옆
    if (fs::exists(cand2)) return cand2;
    return {}; // 못 찾음
}

// ====== auto 로더: 경로 찾고 기존 load_wordlist 재사용 ======
static std::unordered_set<std::string> load_wordlist_auto(const char* name) {
    auto p = locate_file(name);
    if (p.empty()) {
        std::cerr << "[warn] cannot find " << name
                  << " (tried CWD and exe dir)\n";
        return {};
    }
    // 찾은 경로 문자열을 기존 로더에 전달
    const std::string path_str = p.string();
    auto set = load_wordlist(path_str.c_str());
    if (set.empty()) {
        std::cerr << "[warn] loaded 0 entries from " << path_str
                  << " (check encoding/contents)\n";
    } else {
        std::cout << "[info] loaded " << set.size() << " entries from "
                  << path_str << "\n";
    }
    return set;
}

static const std::unordered_set<std::string>& DICT() {
    static const auto dict = load_wordlist_auto("words.txt");
    return dict;
}
static const std::unordered_set<std::string>& STOP() {
    static const auto stop = load_wordlist_auto("stopwords.txt");
    return stop;
}

// 본체: 입력 문자열을 한 번만 스캔하여 토큰화+정규화+필터
// 반환: 사전/불용어/규칙 통과한 "고유한" 단어 집합
static std::unordered_set<std::string> unique_words_fast(const std::string& text, const ProgressFn& on_progress = {}) {
    const size_t n = text.size();
    const auto& dict = DICT();
    const auto& stop = STOP();

    std::unordered_set<std::string> out;
    out.reserve(4096); // 대략적인 초기 버킷 (필요시 조정)

    std::string cur; cur.reserve(32);

    bool in_token = false;
    bool at_sentence_start = true;
    // 고유명사/대문자 판정용 플래그(문자 저장 대신 플래그만)
    bool first_is_upper = false;
    bool seen_lower = false;
    bool all_caps = true;
    bool token_started_at_sentence_start = false;

    size_t next_report = n / 50;
    if (next_report == 0) next_report = 1024 * 64; // 최소 64KB 단위

    for (size_t i=0; i<n; ++i) {
        unsigned char c = (unsigned char)text[i];

        // std::cout << c;

        // Ctrl+Z 무시
        if (c == 0x1A) continue;

        auto commit_token = [&](){
            if (!in_token) return;

            // 1) 한 글자 제외
            if (cur.size() <= 1) { cur.clear(); in_token=false; return; }

            // 2) TitleCase(문장 중간) 제외 + ALL-CAPS 제외
            bool looks_titlecase = first_is_upper && seen_lower;
            bool is_proper_like  = looks_titlecase && !token_started_at_sentence_start;

            if (!is_proper_like && !all_caps) {
                // 3) 사전/스톱워드 필터
                if (!cur.empty() && dict.find(cur)!=dict.end() && stop.find(cur)==stop.end()) {
                    out.insert(cur);
                }
            }
            cur.clear();
            in_token = false;
        };

        // --- 알파벳 ---
        if (ascii_is_alpha(c)) {
            const bool is_upper = (c >= 'A' && c <= 'Z');
            const bool is_lower = (c >= 'a' && c <= 'z');
            char lower = ascii_to_lower(c);

            if (!in_token) {
                in_token = true;
                token_started_at_sentence_start = at_sentence_start;

                // 첫 글자 특성 기록
                first_is_upper = is_upper;
                seen_lower     = is_lower;   // 첫 글자가 소문자라면 곧바로 true
                all_caps       = is_upper;   // 첫 글자가 대문자면 일단 true로 시작
            } else {
                // 진행 중 소문자를 하나라도 보면 ALL-CAPS 해제
                if (is_lower) {
                    seen_lower = true;
                    all_caps   = false;
                }
                // 대문자는 all_caps에 영향 없음(유지)
            }

            cur.push_back(lower);
            at_sentence_start = false;
            continue;
        }
        // --- 내부 연결자: ' 또는 - (정규화: 그대로 저장) ---
        // 바로 뒤가 알파벳이면 단어 내부로 포함
        if ((c=='\'' || c=='-') && in_token && i+1<n && ascii_is_alpha((unsigned char)text[i+1])) {
            cur.push_back((char)c);
            continue;
        }

        // --- UTF-8 ’ (E2 80 99) → ' ---
        if (i+2<n && c==0xE2 &&
            (unsigned char)text[i+1]==0x80 &&
            (unsigned char)text[i+2]==0x99)
        {
            if (in_token && i+3<n && ascii_is_alpha((unsigned char)text[i+3])) {
                cur.push_back('\'');
                i += 2;
                continue;
            }
        }
        // CP1252 ’
        if (c==0x92) {
            if (in_token && i+1<n && ascii_is_alpha((unsigned char)text[i+1])) {
                cur.push_back('\'');
                continue;
            }
        }
        // CP949 ‘/’
        if (i+1<n && c==0xA1) {
            unsigned char c2=(unsigned char)text[i+1];
            if ((c2==0xAE || c2==0xAF) && in_token && i+2<n && ascii_is_alpha((unsigned char)text[i+2])) {
                cur.push_back('\'');
                i += 1;
                continue;
            }
        }

        // 구분자 → 토큰 종료
        commit_token();

        // 문장 시작 판정 (.,!,?)
        if (c=='.' || c=='!' || c=='?') at_sentence_start = true;
        else if (!std::isspace(c))     at_sentence_start = false;

        if (on_progress && (i % next_report == 0)) {
            on_progress(i, n);
        }

    }

    // 마지막 토큰 flush
    if (in_token) {
        // commit_token() inline 복붙 (성능 미세 이득)
        if (cur.size() > 1) {
            bool looks_titlecase = first_is_upper && seen_lower;
            bool is_proper_like  = looks_titlecase && !token_started_at_sentence_start;
            if (!is_proper_like && !all_caps) {
                if (dict.find(cur)!=dict.end() && STOP().find(cur)==STOP().end())
                    out.insert(cur);
            }
        }
    }

    // 마지막 진행률 100% 보장
    if (on_progress) on_progress(n, n);
    return out;
}

static void print_step(const char* msg) {
    std::cout << "[*] " << msg << std::endl;
}

// 간단 타이머
struct StepTimer {
    std::chrono::high_resolution_clock::time_point t0;
    StepTimer(): t0(std::chrono::high_resolution_clock::now()) {}
    double elapsed_ms() const {
        using namespace std::chrono;
        return duration_cast<duration<double, std::milli>>(high_resolution_clock::now()-t0).count();
    }
};

int word_extractor_main(const std::string& input) {
    // I/O 가속
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    StepTimer total;

    print_step("Loading dictionaries...");
    StepTimer t1;
    const auto& dict = DICT();
    const auto& stop = STOP();
    std::cout << "    - words.txt: " << dict.size() << " entries\n";
    std::cout << "    - stopwords.txt: " << stop.size() << " entries\n";
    std::cout << "    (load: " << std::fixed << std::setprecision(1) << t1.elapsed_ms() << " ms)\n";

    print_step("Extracting unique words (tokenize + filter)...");
    StepTimer t2;

    // 진행률 출력 콜백
    auto on_progress = [](size_t processed, size_t total) {
        double pct = total ? (processed * 100.0 / total) : 100.0;
        // 같은 줄에 덮어쓰기
        std::cout << "\r    progress: " << std::setw(6) << std::fixed << std::setprecision(2)
                  << pct << "% (" << processed << "/" << total << ")" << std::flush;
    };

    auto set = unique_words_fast(input, on_progress);
    std::cout << "\r    progress: 100.00% (" << input.size() << "/" << input.size() << ")          \n";
    std::cout << "    (extract: " << std::fixed << std::setprecision(1) << t2.elapsed_ms() << " ms)\n";

    print_step("Sorting & writing output...");
    StepTimer t3;
    std::vector<std::string> v; v.reserve(set.size());
    for (const auto& s : set) v.push_back(s);
    std::sort(v.begin(), v.end());

    // 파일로 저장
    namespace fs = std::filesystem;
    fs::path out = exe_dir() / "vocab.txt";
    std::ofstream fout(out, std::ios::binary);
    fout << "Unique filtered words (" << v.size() << ")\n";
    for (const auto& s : v) { fout << s << '\n'; }
    fout.close();
    std::cout << "    - written: vocab.txt (" << v.size() << " words)\n";
    std::cout << "    (write: " << std::fixed << std::setprecision(1) << t3.elapsed_ms() << " ms)\n";

    std::cout << "[✓] Done. Total: " << std::fixed << std::setprecision(1)
              << total.elapsed_ms() << " ms\n";

    return (int)v.size();
}

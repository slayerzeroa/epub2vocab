#include <cctype>
#include <string>
#include <set>
#include <unordered_set>
#include <fstream>
#include <iostream>

// ========== 정규화 유틸 (그대로) ==========
static void normalize_apostrophes_and_dashes(std::string& s) {
    std::string out; out.reserve(s.size());
    const size_t n = s.size();
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (i + 2 < n && c == 0xE2 && (unsigned char)s[i+1] == 0x80 && (unsigned char)s[i+2] == 0x99)
        { out.push_back('\''); i += 2; continue; }                 // UTF-8 ’ -> '
        if (i + 2 < n && c == 0xE2 && (unsigned char)s[i+1] == 0x80 && (unsigned char)s[i+2] == 0x93)
        { out.push_back('-');  i += 2; continue; }                 // UTF-8 – -> -
        if (c == 0x92) { out.push_back('\''); continue; }          // CP1252 ’ -> '
        if (i + 1 < n && c == 0xA1) {                              // CP949 ‘/’ -> '
            unsigned char c2 = (unsigned char)s[i+1];
            if (c2 == 0xAE || c2 == 0xAF) { out.push_back('\''); i += 1; continue; }
        }
        if (i + 1 < n && c == 0x3F && (unsigned char)s[i+1] == 0x99)
        { out.push_back('\''); i += 1; continue; }                  // 깨짐 '? 0x99' 패턴
        out.push_back((char)c);
    }
    s.swap(out);
}

static inline bool is_sentence_terminator(unsigned char c) {
    return c=='.' || c=='!' || c=='?';
}

// ========== 공통 로더 ==========
static std::unordered_set<std::string> load_wordlist(const char* path) {
    std::unordered_set<std::string> set;
    std::ifstream fin(path, std::ios::binary);
    if (!fin) {
        std::cerr << "[warn] cannot open " << path << " — set is empty.\n";
        return set;
    }
    std::string line;
    while (std::getline(fin, line)) {
        // trim
        while (!line.empty() && (line.back()=='\r' || std::isspace((unsigned char)line.back()))) line.pop_back();
        size_t p = 0; while (p < line.size() && std::isspace((unsigned char)line[p])) ++p;
        if (p) line.erase(0, p);
        if (line.empty()) continue;

        // 정규화 + 소문자화 후 삽입
        normalize_apostrophes_and_dashes(line);
        for (auto& ch : line) ch = (char)std::tolower((unsigned char)ch);
        set.insert(std::move(line));
    }
    return set;
}

// ========== 전역 캐시 ==========
static const std::unordered_set<std::string>& DICT() {
    static const std::unordered_set<std::string> dict = load_wordlist("words.txt");
    return dict;
}
static const std::unordered_set<std::string>& STOP() {
    static const std::unordered_set<std::string> stop = load_wordlist("stopwords.txt");
    return stop;
}

// ========== 요구사항 반영: proper 제외 + 한 글자 제외 + words 화이트리스트 + stopwords 블랙리스트 ==========
static std::set<std::string> unique_words(std::string text) {
    normalize_apostrophes_and_dashes(text);

    auto is_alpha = [](unsigned char x){ return std::isalpha(x) != 0; };
    auto to_lower = [](unsigned char x){ return (char)std::tolower(x); };

    std::set<std::string> words;
    std::string cur_norm, cur_orig;
    const size_t n = text.size();

    bool in_token = false;
    bool at_sentence_start = true;
    bool token_started_at_sentence_start = false;

    auto flush_token = [&](){
        if (!in_token) return;

        // (1) 한 글자 제외
        if (cur_norm.size() <= 1) { cur_norm.clear(); cur_orig.clear(); in_token=false; return; }

        // (2) 고유명사(문장 중간 TitleCase) 제외 + ALL-CAPS 제외
        bool has_lower=false, first_is_upper=false, all_caps=true;
        for (size_t i=0;i<cur_orig.size();++i){
            unsigned char c = (unsigned char)cur_orig[i];
            if (std::isalpha(c)) {
                if (!std::isupper(c)) all_caps = false;
                if (i==0) first_is_upper = std::isupper(c)!=0;
                if (std::islower(c)) has_lower = true;
            }
        }
        bool looks_titlecase = first_is_upper && has_lower;            // e.g., Google, Seoul
        bool is_proper_like  = looks_titlecase && !token_started_at_sentence_start;

        // (3) words.txt 화이트리스트 + stopwords.txt 블랙리스트
        const auto& dict = DICT();
        const auto& stop = STOP();
        bool in_dict = (dict.find(cur_norm) != dict.end());
        bool in_stop = (stop.find(cur_norm) != stop.end());

        // 최종 포함 조건: (proper 아님) AND (ALL-CAPS 아님) AND (사전에 있음) AND (스톱워드가 아님)
        if (!is_proper_like && !all_caps && in_dict && !in_stop) {
            words.insert(cur_norm);
        }

        cur_norm.clear(); cur_orig.clear(); in_token=false;
    };

    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == 0x1A) continue; // Ctrl+Z 무시

        if (is_alpha(c)) {
            if (!in_token) { in_token=true; token_started_at_sentence_start = at_sentence_start; }
            cur_orig.push_back((char)c);
            cur_norm.push_back(to_lower(c));
            at_sentence_start = false;
            continue;
        }

        // 내부 연결자 포함
        if ((c == '-' || c == '\'') &&
            in_token && i + 1 < n && is_alpha((unsigned char)text[i+1]))
        {
            cur_orig.push_back((char)c);
            cur_norm.push_back((char)c);
            continue;
        }

        // 토큰 종료
        flush_token();

        // 문장 시작 판정 갱신
        if (c=='.' || c=='!' || c=='?') at_sentence_start = true;
        else if (!std::isspace(c))      at_sentence_start = false;
    }
    flush_token();

    return words;
}



static void dump_bytes(const std::string& s){
    std::cout << "bytes:";
    for(unsigned char b: s) std::cout << " " << std::hex << std::uppercase << (int)b;
    std::cout << std::dec << "\n";
}


int main() {
    std::string text;
    std::cout << "Enter/Paste text (end with Ctrl+D or Ctrl+Z):\n";
    std::string line;
    while (std::getline(std::cin, line)) {
        text += line + "\n";
    }

    // dump_bytes(text);


    auto words = unique_words(text);
    std::cout << "Unique words found (" << words.size() << "):\n";
    for (const auto& w : words) {
        std::cout << w << "\n";
    }
    return 0;
}
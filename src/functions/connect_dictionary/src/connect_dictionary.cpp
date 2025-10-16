#include <curl/curl.h>
#include <string>
#include <iostream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>

#include <fstream>
#include <sstream>
#include <unordered_map>
#include <stdexcept>  // std::runtime_error
#include "connect_dictionary.hpp"


#include <filesystem>
#ifdef _WIN32
  #include <windows.h>
#endif

static std::filesystem::path exe_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return n ? std::filesystem::path(buf).parent_path()
             : std::filesystem::current_path();
#else
    // Linux/macOS는 /proc/self/exe 등으로 보강하거나,
    // 간단히 현재 작업 디렉토리를 쓰세요.
    return std::filesystem::current_path();
#endif
}


std::unordered_map<std::string,std::string> load_env(const std::string& path) {
    std::unordered_map<std::string,std::string> env;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        env[key] = val;
    }
    return env;
}

size_t write_cb(void* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)ptr, size*nmemb);
    return size*nmemb;
}

std::string custom_get(std::string url) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL* curl = curl_easy_init();
    std::string response;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
    return response;
}


void save_to_file(const std::string& path, const std::string& content) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) throw std::runtime_error("failed to create: " + path);
    ofs << content;
    ofs.close();
}



std::filesystem::path envPath = exe_dir() / ".env";
auto env = load_env(envPath.u8string()); // 프로젝트 루트 기준 상대경로
std::string DICTIONARY_KEY = env["DICTIONARY_KEY"];

// 주어진 단어의 짧은 정의(shortdef)를 가져와서 출력 + 품사(fl)
std::string print_definition(const std::string& word) {
    if (DICTIONARY_KEY.empty()) {
        std::cerr << "Error: DICTIONARY_KEY is not set in .env file.\n";
        return std::string("No DICTIONARY_KEY provided.");
    }
    std::cout << "Looking up: " << word << "\n";

    std::string url = "https://www.dictionaryapi.com/api/v3/references/collegiate/json/"
                      + word + "?key=" + DICTIONARY_KEY;

    // std::cout << "Request URL: " << url << "\n";

    std::string raw = custom_get(url);
    nlohmann::json j = nlohmann::json::parse(raw);
    // std::cout << j.dump(2) << "\n";

    if (!j.is_array() || j.empty()) {
        std::cout << "No definitions found for \"" << word << "\"\n";
        return std::string("No definitions found.");
    }

    std::string response;

    // 자동완성 제안(문자열 배열) 대응
    bool all_strings = true;
    for (const auto& e : j) { if (!e.is_string()) { all_strings = false; break; } }
    if (all_strings) {
        std::cout << "Did you mean:\n";
        for (const auto& s : j) std::cout << " - " << s.get<std::string>() << "\n";
        return std::string("No exact match found. Suggestions provided.");
    }

    // Merriam-Webster는 하나의 단어에 여러 entry가 있을 수 있음
    for (size_t i = 0; i < j.size(); ++i) {
        if (i >= 3) { // 최대 3개 세트만 출력
            std::cout << "... (more definitions available)\n";
            break;
        }
        const auto& entry = j[i];
        if (!entry.is_object() || !entry.contains("shortdef")) continue;

        // 품사(fl)와 표제어(hwi.hw) 추출
        std::string pos;
        if (entry.contains("fl") && entry["fl"].is_string()) pos = entry["fl"].get<std::string>();

        std::string headword;
        if (entry.contains("hwi") && entry["hwi"].is_object()
            && entry["hwi"].contains("hw") && entry["hwi"]["hw"].is_string()) {
            headword = entry["hwi"]["hw"].get<std::string>();
        };

        if (i == 0) {
            response += "Target Word: " + word + "\n";
        };
        response += "Definition Set " + std::to_string(i + 1) + ":\n";
        response += "Headword: " + headword + "\n";
        response += "Part of Speech: " + pos + "\n";
        response += "Definitions:\n";
        for (const auto& def : entry["shortdef"]) {
            if (def.is_string()) response += " - " + def.get<std::string>() + "\n";
        }

        // std::cout << "---- Definition Set " << (i + 1) << " ----\n";
        // if (!headword.empty()) std::cout << "Headword: " << headword << "\n";
        // if (!pos.empty())      std::cout << "Part of Speech: " << pos << "\n";

        // for (const auto& def : entry["shortdef"]) {
        //     if (def.is_string()) std::cout << "- " << def.get<std::string>() << "\n";
        // }
    }
    return response;
}


// int connect_dictionary(int argc, char* argv[]) {
//     if (argc < 2) {
//         std::cerr << "Usage: " << argv[0] << " <word>\n";
//         return 1;
//     }

//     std::string word = argv[1];

//     std::string response = print_definition(word); // ← print_definition은 std::string 반환으로 통일

//     std::filesystem::path out = exe_dir() / "definition.txt";
//     std::ofstream ofs(out, std::ios::binary);
//     ofs << response;
//     ofs.close();

//     return 0;
// }


int connect_dictionary(std::string word) {
    std::string response = print_definition(word); // ← print_definition은 std::string 반환으로 통일

    std::filesystem::path out = exe_dir() / "definition.txt";
    std::ofstream ofs(out, std::ios::binary);
    ofs << response;
    ofs.close();

    return 0;
}




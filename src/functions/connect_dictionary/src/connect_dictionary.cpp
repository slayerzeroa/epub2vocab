#include <curl/curl.h>
#include <string>
#include <iostream>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>

#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <stdexcept>  // std::runtime_error

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


auto env = load_env("./src/.env"); // 프로젝트 루트 기준 상대경로
std::string DICTIONARY_KEY = env["DICTIONARY_KEY"];

// 주어진 단어의 짧은 정의(shortdef)를 가져와서 출력 + 품사(fl)
void print_definition(const std::string& word) {
    if (DICTIONARY_KEY.empty()) {
        std::cerr << "Error: DICTIONARY_KEY is not set in .env file.\n";
        return;
    }
    std::cout << "Looking up: " << word << "\n";

    std::string url = "https://www.dictionaryapi.com/api/v3/references/collegiate/json/"
                      + word + "?key=" + DICTIONARY_KEY;

    std::cout << "Request URL: " << url << "\n";

    std::string raw = custom_get(url);
    nlohmann::json j = nlohmann::json::parse(raw);
    // std::cout << j.dump(2) << "\n";

    if (!j.is_array() || j.empty()) {
        std::cout << "No definitions found for \"" << word << "\"\n";
        return;
    }

    // 자동완성 제안(문자열 배열) 대응
    bool all_strings = true;
    for (const auto& e : j) { if (!e.is_string()) { all_strings = false; break; } }
    if (all_strings) {
        std::cout << "Did you mean:\n";
        for (const auto& s : j) std::cout << " - " << s.get<std::string>() << "\n";
        return;
    }

    // Merriam-Webster는 하나의 단어에 여러 entry가 있을 수 있음
    for (size_t i = 0; i < j.size(); ++i) {
        const auto& entry = j[i];
        if (!entry.is_object() || !entry.contains("shortdef")) continue;

        // 품사(fl)와 표제어(hwi.hw) 추출
        std::string pos;
        if (entry.contains("fl") && entry["fl"].is_string()) pos = entry["fl"].get<std::string>();

        std::string headword;
        if (entry.contains("hwi") && entry["hwi"].is_object()
            && entry["hwi"].contains("hw") && entry["hwi"]["hw"].is_string()) {
            headword = entry["hwi"]["hw"].get<std::string>();
        }

        std::cout << "---- Definition Set " << (i + 1) << " ----\n";
        if (!headword.empty()) std::cout << "Headword: " << headword << "\n";
        if (!pos.empty())      std::cout << "Part of Speech: " << pos << "\n";

        for (const auto& def : entry["shortdef"]) {
            if (def.is_string()) std::cout << "- " << def.get<std::string>() << "\n";
        }
    }
}


int main() {
    // Merriam-Webster Collegiate Dictionary API key
    // // 디버깅
    // std::cout << "Current working directory: " << std::filesystem::current_path().u8string() << "\n";
    // std::cout << "Dictionary Key from .env: " << DICTIONARY_KEY << "\n";

    std::string word;
    std::cout << "Enter a word to look up: ";
    std::cin >> word;
    print_definition(word);
    return 0;
}




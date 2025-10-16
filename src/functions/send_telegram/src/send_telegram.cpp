#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <unordered_map>

#include "send_telegram.hpp"

#ifdef _WIN32
  #include <windows.h>
#endif

// 실행 파일이 있는 폴더 경로 구하기
static std::filesystem::path exe_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buf).parent_path();
#else
    // Linux/macOS는 /proc/self/exe 등을 활용하거나, 기본은 현재 작업 디렉토리
    return std::filesystem::current_path();
#endif
}

// .env 파일 로드 (key=value)
static std::unordered_map<std::string, std::string> load_env(const std::filesystem::path &path)
{
    std::unordered_map<std::string, std::string> env;
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Cannot open .env file at: " << path << "\n";
        return env;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);

        // 공백 제거
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        val.erase(0, val.find_first_not_of(" \t\r\n"));
        val.erase(val.find_last_not_of(" \t\r\n") + 1);

        env[key] = val;
    }
    return env;
}



// Telegram API KEY 가져오기
std::filesystem::path envPathTelegram = exe_dir() / ".env";
auto envTelegram = load_env(envPathTelegram.u8string());
std::string TELEGRAM_API_KEY = envTelegram["TELEGRAM_API_KEY"];
std::string TELEGRAM_CHAT_ID = envTelegram["TELEGRAM_CHAT_ID"];


// 1) sendMessage: 긴 텍스트는 4096자 단위로 분할 전송
bool telegram_send_message(const std::string& text) {
    if (TELEGRAM_API_KEY.empty() || TELEGRAM_CHAT_ID.empty()) return false;

    const size_t LIMIT = 4096;
    size_t offset = 0;
    bool ok_all = true;

    while (offset < text.size()) {
        std::string chunk = text.substr(offset, LIMIT);
        offset += chunk.size();

        // 엔드포인트
        std::string url = "https://api.telegram.org/bot" + TELEGRAM_API_KEY + "/sendMessage";

        CURL* curl = curl_easy_init();
        if (!curl) return false;

        // POST fields
        std::string postfields = "chat_id=" + TELEGRAM_CHAT_ID + "&text=";
        // URL-encode
        char* enc = curl_easy_escape(curl, chunk.c_str(), (int)chunk.size());
        if (!enc) { curl_easy_cleanup(curl); return false; }
        postfields += enc;
        curl_free(enc);

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);

        // 보안 옵션(필요 시)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "sendMessage error: " << curl_easy_strerror(res) << "\n";
            ok_all = false;
        }
        curl_easy_cleanup(curl);
    }
    return ok_all;
}


// int main() {
//     // // 환경변수에서 읽기 (또는 .env 로딩 로직 사용)
//     // std::string BOT = TELEGRAM_API_KEY;
//     // std::string CHAT = TELEGRAM_CHAT_ID;

//     // 1) definition.txt 내용을 텍스트로 보내기
//     std::ifstream ifs("definition.txt", std::ios::binary);
//     if (!ifs) {
//         std::cerr << "definition.txt not found\n";
//         return 1;
//     }
//     std::ostringstream oss;
//     oss << ifs.rdbuf();
//     std::string content = oss.str();

//     // 짧으면 메시지로, 길면 파일로
//     telegram_send_message(content);

//     return 0;
// }
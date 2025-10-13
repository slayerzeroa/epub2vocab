#include "py_runner/py_runner.hpp"
#include <filesystem>
#include <iostream>
#ifdef _WIN32
#  include <windows.h>
#endif

namespace fs = std::filesystem;

// 1) 하드코딩한 파이썬 경로
static std::wstring hardcoded_python() {
    return L"C:\\Users\\slaye\\anaconda3\\python.exe";  // ← 네 경로
}

// 2) PATH, py.exe 등도 탐색하는 보조
static std::wstring search_in_path(const wchar_t* name) {
    wchar_t buf[MAX_PATH];
    DWORD n = SearchPathW(nullptr, name, nullptr, MAX_PATH, buf, nullptr);
    return n ? std::wstring(buf) : L"";
}

static fs::path get_exe_dir() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return n ? fs::path(buf).parent_path() : fs::current_path();
#else
    return fs::current_path();
#endif
}

static std::wstring Q(const fs::path& p) { return L"\"" + p.wstring() + L"\""; }

// 3) 파이썬 찾기: 하드코딩 → ENV → PATH 순
static std::wstring find_python() {
    // 하드코딩 우선
    if (fs::exists(hardcoded_python()))
        return hardcoded_python();

    // 환경변수(있으면)
    if (const wchar_t* env = _wgetenv(L"EPUB2VOCAB_PYTHON")) {
        if (fs::exists(env)) return env;
    }

    // PATH 탐색
    if (auto py = search_in_path(L"py.exe"); !py.empty())     return py;
    if (auto py = search_in_path(L"python.exe"); !py.empty()) return py;

    return L""; // 못 찾음
}

int run_lemmatizer(const fs::path& exe_dir,
                   const fs::path& vocab_txt_in,
                   const fs::path& out_txt_in)
{
    fs::path exeDir = exe_dir.empty() ? get_exe_dir() : exe_dir;

    // 기본 경로들
    fs::path vocab = vocab_txt_in.empty() ? (exeDir / "vocab.txt") : vocab_txt_in;
    fs::path out   = out_txt_in.empty()   ? (exeDir / "vocab_lemma.txt") : out_txt_in;

    // 프로젝트 루트 추정: build/Release → ..\.. → 루트
    fs::path root = exeDir.parent_path().parent_path();
    fs::path script = root / "py" / "lemmatize_list.py";

    if (!fs::exists(vocab)) {
        std::cerr << "[warn] not found: " << vocab.string() << "\n";
        return 1;
    }
    if (!fs::exists(script)) {
        std::cerr << "[warn] not found: " << script.string() << "\n";
        return 2;
    }

    std::wstring py = find_python();
    std::wstring cmd = L"\"" + py + L"\" " +
                    (py.rfind(L"py.exe") != std::wstring::npos ? L"-3 " : L"") +
                    Q(script) + L" " + Q(vocab) + L" --out " + Q(out);

#ifdef _WIN32
    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cmdline = cmd; // CreateProcessW는 수정 가능한 버퍼 요구
    BOOL ok = CreateProcessW(
        nullptr, cmdline.data(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr,
        exeDir.wstring().c_str(), // CWD = exe 폴더
        &si, &pi
    );
    if (!ok) {
        std::cerr << "[error] CreateProcessW failed: " << GetLastError() << "\n";
        return 3;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0; GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (exitCode == 0)
        std::cout << "[ok] wrote " << out.string() << "\n";
    else
        std::cerr << "[error] lemmatizer exit code: " << exitCode << "\n";
    return (int)exitCode;
#else
    int rc = std::system(std::string(cmd.begin(), cmd.end()).c_str());
    return rc;
#endif
}

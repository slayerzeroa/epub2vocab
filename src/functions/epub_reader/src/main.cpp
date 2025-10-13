#include <zip.h>
#include <pugixml.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <fstream>


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


// ---- zip에서 파일 읽기 ----
static std::string read_zip_entry(zip_t* z, const std::string& name) {
    zip_stat_t st;
    if (zip_stat(z, name.c_str(), 0, &st) != 0)
        throw std::runtime_error("zip_stat failed: " + name);

    zip_file_t* f = zip_fopen(z, name.c_str(), 0);
    if (!f)
        throw std::runtime_error("zip_fopen failed: " + name);

    std::string buf;
    buf.resize(static_cast<size_t>(st.size));
    zip_int64_t n = zip_fread(f, buf.data(), st.size);
    zip_fclose(f);

    if (n < 0 || n != static_cast<zip_int64_t>(st.size))
        throw std::runtime_error("zip_fread incomplete: " + name);
    return buf;
}

// ---- 경로 유틸 ----
static std::string dirname_of(const std::string& p) {
    auto pos = p.find_last_of("/\\");
    if (pos == std::string::npos) return std::string();
    return p.substr(0, pos);
}

static void normalize_path(std::vector<std::string>& parts) {
    std::vector<std::string> out;
    for (auto& s : parts) {
        if (s.empty() || s == ".") continue;
        if (s == "..") {
            if (!out.empty()) out.pop_back();
        } else {
            out.push_back(s);
        }
    }
    parts.swap(out);
}

static std::string join_path(const std::string& base, const std::string& rel) {
    if (rel.empty()) return base;
    if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) return rel; // root-like
    if (base.empty()) return rel;

    std::string tmp = base + "/" + rel;
    // normalize
    std::vector<std::string> parts;
    size_t i=0;
    while (i < tmp.size()) {
        size_t j = tmp.find('/', i);
        if (j == std::string::npos) j = tmp.size();
        parts.emplace_back(tmp.substr(i, j - i));
        i = j + 1;
    }
    normalize_path(parts);
    std::string res;
    for (size_t k=0; k<parts.size(); ++k) {
        if (k) res.push_back('/');
        res += parts[k];
    }
    return res;
}

// ---- XHTML 텍스트 추출 (pugixml로 단순 텍스트만) ----
// script/style 등은 스킵
static bool is_hidden_tag(const char* name) {
    // 소문자 비교 가정 (pugi는 원문 그대로 반환)
    return std::string(name) == "script" || std::string(name) == "style";
}

static void collect_text_recursive(const pugi::xml_node& node, std::string& out) {
    // 텍스트 노드
    if (node.type() == pugi::node_pcdata || node.type() == pugi::node_cdata) {
        out.append(node.value());
        return;
    }
    // 엘리먼트
    if (node.type() == pugi::node_element) {
        const char* nm = node.name();
        if (nm && is_hidden_tag(nm)) return; // skip script/style

        // 자식 순회
        for (pugi::xml_node ch = node.first_child(); ch; ch = ch.next_sibling()) {
            collect_text_recursive(ch, out);
        }
        // 블록성 태그 뒤에 공백 한 칸 정도 추가해서 단어 붙음 방지 (단순 처리)
        static const char* blockish[] = {
            "p","div","h1","h2","h3","h4","h5","h6","li","ul","ol","section","article","br"
        };
        if (nm) {
            for (auto b : blockish) {
                if (std::string(nm) == b) {
                    out.push_back(' ');
                    break;
                }
            }
        }
        return;
    }
    // 그 외 노드 타입도 자식 순회
    for (pugi::xml_node ch = node.first_child(); ch; ch = ch.next_sibling()) {
        collect_text_recursive(ch, out);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: epub_reader <file.epub>\n";
        return 1;
    }
    const char* path = argv[1];

    int errcode = 0;
    zip_t* z = zip_open(path, ZIP_RDONLY, &errcode);
    if (!z) {
        zip_error_t ze;
        zip_error_init_with_code(&ze, errcode);
        std::cerr << "zip_open failed: " << zip_error_strerror(&ze) << "\n";
        zip_error_fini(&ze);
        return 2;
    }

    try {
        // (1) mimetype 확인
        std::string mimetype = read_zip_entry(z, "mimetype");
        while (!mimetype.empty() && (mimetype.back() == '\n' || mimetype.back() == '\r'))
            mimetype.pop_back();

        std::cout << "[info] mimetype: " << mimetype << "\n";
        if (mimetype != "application/epub+zip") {
            std::cerr << "[warn] unexpected mimetype!\n";
        }

        // (2) container.xml 읽기
        std::string container_xml = read_zip_entry(z, "META-INF/container.xml");
        std::cout << "[info] container.xml size: " << container_xml.size() << " bytes\n";

        // (3) pugixml로 OPF 경로 추출
        pugi::xml_document doc;
        if (!doc.load_string(container_xml.c_str()))
            throw std::runtime_error("Failed to parse container.xml");

        auto rootfile = doc.select_node("/container/rootfiles/rootfile");
        if (!rootfile)
            throw std::runtime_error("No <rootfile> element");

        std::string opf_path = rootfile.node().attribute("full-path").as_string();
        std::cout << "[info] OPF path: " << opf_path << "\n";

        // (3) OPF 읽기 + 파싱
        std::string opf_content = read_zip_entry(z, opf_path);
        pugi::xml_document opfdoc;
        if (!opfdoc.load_string(opf_content.c_str()))
            throw std::runtime_error("Failed to parse OPF");

        // (4) manifest: id -> href 매핑
        std::unordered_map<std::string, std::string> id_to_href;
        pugi::xml_node manifest = opfdoc.child("package").child("manifest");
        for (pugi::xml_node item = manifest.child("item"); item; item = item.next_sibling("item")) {
            std::string id   = item.attribute("id").as_string();
            std::string href = item.attribute("href").as_string();
            if (!id.empty() && !href.empty()) id_to_href[id] = href;
        }

        // (5) spine: itemref(idref) 순서 수집
        std::vector<std::string> spine_hrefs;
        pugi::xml_node spine = opfdoc.child("package").child("spine");
        for (pugi::xml_node ir = spine.child("itemref"); ir; ir = ir.next_sibling("itemref")) {
            std::string idref = ir.attribute("idref").as_string();
            auto it = id_to_href.find(idref);
            if (it != id_to_href.end()) spine_hrefs.push_back(it->second);
        }

        // (6) 컨텐츠 읽어서 텍스트 추출 → 앞 N자 출력
        std::string opf_dir = dirname_of(opf_path);
        std::string all_text;
        // const size_t LIMIT = 2000; // 원하는 미리보기 글자수

        for (const auto& rel : spine_hrefs) {
            std::string entry = join_path(opf_dir, rel);
            // 일부 OPF는 spine에 image/css 등 잡히기도 하므로 실패해도 통과
            try {
                std::string xhtml = read_zip_entry(z, entry);
                // XHTML 파싱
                pugi::xml_document hdoc;
                if (!hdoc.load_string(xhtml.c_str())) continue;

                // 본문 루트 후보: <html> -> <body>
                pugi::xml_node html = hdoc.child("html");
                pugi::xml_node root = html ? html.child("body") : hdoc;

                // 텍스트 수집
                collect_text_recursive(root, all_text);

            } catch (...) {
                // 무시하고 다음 항목
            }
        }

        // 앞 LIMIT 글자 출력
        if (all_text.empty()) {
            std::cout << "[warn] No XHTML text collected from spine.\n";
        } else {
            // 연속 공백 정리(선택)
            auto squish = [](std::string& s){
                bool prev_space=false; size_t w=0;
                for (size_t i=0;i<s.size();++i){
                    char c = s[i];
                    bool is_space = (c==' ' || c=='\n' || c=='\r' || c=='\t');
                    if (is_space) {
                        if (!prev_space) s[w++] = ' ';
                        prev_space = true;
                    } else {
                        s[w++] = c; prev_space = false;
                    }
                }
                s.resize(w);
            };
            squish(all_text);
            // 파일로 저장
            namespace fs = std::filesystem;
            fs::path out = exe_dir() / "book_text.txt";
            std::ofstream ofs(out, std::ios::binary);

            if (!ofs) throw std::runtime_error("failed to create: " + out.string());
            ofs << all_text;
            ofs.close();

            std::cout << "[info] Saved full text to book_text.txt\n";
            // // 미리보기 출력 (선택)
            // std::cout << all_text.substr(0, 1000) << "\n"; // 앞 1000자만 콘솔 미리보기
        }


        zip_close(z);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[error] " << e.what() << "\n";
        zip_close(z);
        return 3;
    }
}

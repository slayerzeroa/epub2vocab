#include <zip.h>
#include <pugixml.hpp>
#include "epub_reader.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <algorithm>
#include <fstream>

// 디버그용
#include <filesystem>

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


std::string extract_epub_text(const std::string& epub_path) {
    int errcode = 0;

    // // 디버그용
    // std::cerr << "[cwd] " << std::filesystem::current_path() << "\n";
    // std::cerr << "[try] " << std::filesystem::absolute(epub_path) << "\n";

    zip_t* z = zip_open(epub_path.c_str(), ZIP_RDONLY, &errcode);
    if (!z) {
        zip_error_t ze;
        zip_error_init_with_code(&ze, errcode);
        std::string msg = "zip_open failed: " + std::string(zip_error_strerror(&ze));
        zip_error_fini(&ze);
        throw std::runtime_error(msg);
    }

    std::string all_text;
    try {
        std::string mimetype = read_zip_entry(z, "mimetype");
        while (!mimetype.empty() && (mimetype.back() == '\n' || mimetype.back() == '\r'))
            mimetype.pop_back();

        if (mimetype != "application/epub+zip") {
            std::cerr << "[warn] unexpected mimetype!\n";
        }

        // container.xml → OPF path
        std::string container_xml = read_zip_entry(z, "META-INF/container.xml");
        pugi::xml_document doc;
        if (!doc.load_string(container_xml.c_str()))
            throw std::runtime_error("Failed to parse container.xml");
        auto rootfile = doc.select_node("/container/rootfiles/rootfile");
        if (!rootfile)
            throw std::runtime_error("No <rootfile> element");
        std::string opf_path = rootfile.node().attribute("full-path").as_string();

        // OPF 읽기
        std::string opf_content = read_zip_entry(z, opf_path);
        pugi::xml_document opfdoc;
        if (!opfdoc.load_string(opf_content.c_str()))
            throw std::runtime_error("Failed to parse OPF");

        // manifest / spine 파싱
        std::unordered_map<std::string, std::string> id_to_href;
        pugi::xml_node manifest = opfdoc.child("package").child("manifest");
        for (pugi::xml_node item = manifest.child("item"); item; item = item.next_sibling("item")) {
            std::string id   = item.attribute("id").as_string();
            std::string href = item.attribute("href").as_string();
            if (!id.empty() && !href.empty()) id_to_href[id] = href;
        }

        std::vector<std::string> spine_hrefs;
        pugi::xml_node spine = opfdoc.child("package").child("spine");
        for (pugi::xml_node ir = spine.child("itemref"); ir; ir = ir.next_sibling("itemref")) {
            std::string idref = ir.attribute("idref").as_string();
            auto it = id_to_href.find(idref);
            if (it != id_to_href.end()) spine_hrefs.push_back(it->second);
        }

        // spine 순서대로 모든 텍스트 수집
        std::string opf_dir = dirname_of(opf_path);
        for (const auto& rel : spine_hrefs) {
            std::string entry = join_path(opf_dir, rel);
            try {
                std::string xhtml = read_zip_entry(z, entry);
                pugi::xml_document hdoc;
                if (!hdoc.load_string(xhtml.c_str())) continue;
                pugi::xml_node html = hdoc.child("html");
                pugi::xml_node root = html ? html.child("body") : hdoc;
                collect_text_recursive(root, all_text);
            } catch (...) {
                // 무시하고 계속
            }
        }

        zip_close(z);

        // 연속 공백 압축
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

        return all_text;
    }
    catch (...) {
        zip_close(z);
        throw;  // 예외 다시 던짐
    }
}
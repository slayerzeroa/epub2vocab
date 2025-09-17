#include <zip.h>
#include <pugixml.hpp>

#include <iostream>
#include <string>
#include <stdexcept>


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

    if (n != static_cast<zip_int64_t>(st.size))
        throw std::runtime_error("zip_fread incomplete: " + name);
    return buf;
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

        zip_close(z);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[error] " << e.what() << "\n";
        zip_close(z);
        return 3;
    }
}

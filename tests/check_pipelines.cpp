#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

bool filesEqual(const fs::path& a, const fs::path& b) {
    if (fs::file_size(a) != fs::file_size(b)) {
        return false;
    }

    std::ifstream fa(a, std::ios::binary);
    std::ifstream fb(b, std::ios::binary);
    if (!fa || !fb) {
        return false;
    }
    return std::equal(std::istreambuf_iterator<char>(fa), std::istreambuf_iterator<char>(),
                      std::istreambuf_iterator<char>(fb));
}

bool checkImage(const fs::path& dir, const std::string& name, size_t index, size_t total) {
    const fs::path serial  = dir / ("serial-" + name);
    const fs::path pthread = dir / ("pthread-" + name);
    const fs::path tbb     = dir / ("tbb-" + name);

    for (const fs::path& p : {serial, pthread, tbb}) {
        if (!fs::is_regular_file(p)) {
            std::cerr << "\nFile '" << p.filename().string() << "' does not exist\n";
            return false;
        }
    }

    if (!filesEqual(serial, pthread)) {
        std::cerr << "\nFiles '" << serial.filename().string() << "' and '" << pthread.filename().string()
                  << "' don't match\n";
        return false;
    }

    if (!filesEqual(serial, tbb)) {
        std::cerr << "\nFiles '" << serial.filename().string() << "' and '" << tbb.filename().string()
                  << "' don't match\n";
        return false;
    }

    std::cout << "\rChecking images: " << index << '/' << total << std::flush;
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <data_dir>\n";
        return 1;
    }

    const fs::path dir = argv[1];
    static const std::regex source_image("^[0-9]+\\.png$");

    std::vector<std::string> names;
    for (const fs::directory_entry& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && std::regex_match(entry.path().filename().string(), source_image)) {
            names.push_back(entry.path().filename().string());
        }
    }
    std::sort(names.begin(), names.end());

    if (names.empty()) {
        std::cerr << "No source image found in '" << dir.string() << "'\n";
        return 1;
    }

    bool ok = true;
    for (size_t i = 0; i < names.size(); i++) {
        ok = checkImage(dir, names[i], i + 1, names.size()) && ok;
    }

    std::cout << '\n';
    return ok ? 0 : 1;
}

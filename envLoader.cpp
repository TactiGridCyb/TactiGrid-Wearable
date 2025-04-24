#include <fstream>
#include <string>
#include <cstdlib>

void load_env(const std::string& path = ".env") {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

#ifdef _WIN32
        std::string env_entry = key + "=" + value;
        _putenv(env_entry.c_str()); // works with MinGW g++
#else
        setenv(key.c_str(), value.c_str(), 1);
#endif
    }
}
#include <cstdlib>
#include <iostream>
#include "envLoader.cpp"

int main() {
    load_env();

    const char* ssid = std::getenv("WIFI_SSID");
    const char* pass = std::getenv("WIFI_PASS");

    std::cout << (ssid ? ssid : "No SSID") << std::endl;
    std::cout << (pass ? pass : "No PASS") << std::endl;

    return 0;
}

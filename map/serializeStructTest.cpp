// #include <iostream>
// #include <iomanip>
// #include <tuple>
// #include <cstring>
// #include "../envLoader.cpp"

// struct GPSCoord {
//     float lat1;
//     float lon1;
//     float lat2;
//     float lon2;
// };



// int main() {
//     GPSCoord g = {2.1f, 3.2f, 4.8f, 4.5f};
//     char* serizlisedStruct = reinterpret_cast<char*>(&g);
//     std::cout << serizlisedStruct << std::endl;

//     GPSCoord* newG = reinterpret_cast<GPSCoord*>(serizlisedStruct);

//     load_env("../.env");

//     std::cout << env_map["WIFI_SSID"].c_str() << std::endl;
//     std::cout << newG->lon2 << std::endl;
//     return 0;
// }



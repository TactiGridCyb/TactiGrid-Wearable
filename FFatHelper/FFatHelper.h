
#pragma once

#include <FFat.h>

class FFatHelper
{
    public:
    static bool saveFile(const uint8_t* data, size_t len, const char* filePath);
    static bool deleteFile(const char* filePath);
    static bool writeToFile(const char* filePath, const char* content);
    static bool isFileExisting(const char* filePath);
};
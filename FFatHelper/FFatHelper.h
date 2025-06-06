
#pragma once

#include <FFat.h>

class FFatHelper
{
    public:
    static bool saveFile(const uint8_t* data, size_t len, const char* filePath);
    static bool deleteFile(const char* filePath);
    static bool writeToFile(const char* filePath, const char* content);
    static bool isFileExisting(const char* filePath);
    static void listFiles(const char* path = "/", uint8_t depth = 0);
    static bool appendJsonObjectToFile(const char* filePath, const char* jsonObject);
    static bool readFile(const char* filePath, String& outContent);
    static void removeFilesIncludeWords(const char* filterWord, const char* filesType);
};
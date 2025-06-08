
#pragma once

#include <FFat.h>
#include <ArduinoJson.h>

class FFatHelper
{
    public:
    static bool saveFile(const uint8_t* data, size_t len, const char* filePath);
    static bool deleteFile(const char* filePath);
    static bool writeToFile(const char* filePath, const char* content);
    static bool isFileExisting(const char* filePath);
    static void listFiles(const char* path = "/", uint8_t depth = 0);
    static bool appendRegularJsonObjectToFile(const char* filePath, JsonDocument& json);
    static bool readFile(const char* filePath, String& outContent);
    static void removeFilesIncludeWords(const char* filterWord, const char* filesType);
    static void removeFilesStartingWith(const char* prefix);
    static bool initializeLogFile(const char* path, float intervalMS, String missionID);

};
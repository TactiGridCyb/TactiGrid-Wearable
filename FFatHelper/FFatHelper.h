
#pragma once

#include <FFat.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>


#define LOCK_FFAT() \
    if (FFatHelper::fileMutex) xSemaphoreTake(FFatHelper::fileMutex, portMAX_DELAY)

#define UNLOCK_FFAT() \
    if (FFatHelper::fileMutex) xSemaphoreGive(FFatHelper::fileMutex)

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
    static bool appendJSONEvent(const char* filePath, JsonDocument& event);

    static void begin();

    private:
    static SemaphoreHandle_t fileMutex;
    static bool isInitialized;

};
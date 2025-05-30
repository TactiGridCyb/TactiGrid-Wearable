#include "FFatHelper.h"

bool FFatHelper::saveFile(const uint8_t* data, size_t len, const char* filePath)
{
    File file = FFat.open(filePath, FILE_WRITE);
    
    if (!file) {
        return false;
    }
    
    file.write(data, len);
    file.close();

    return true;
}

bool FFatHelper::deleteFile(const char* filePath)
{
    if (FFat.exists(filePath)) {
        return FFat.remove(filePath);
    }

  return false;
}

bool FFatHelper::writeToFile(const char* filePath, const char* content)
{
    File file = FFat.open(filePath, FILE_APPEND);

    if (!file) {
        Serial.println("Failed to open file for writing!");
        return false;
    } else {
        Serial.println("File existing");
        file.println(content);
        Serial.println("Closing file");
        file.close();
        return true;
    }
}

bool FFatHelper::isFileExisting(const char* filePath)
{
    return FFat.exists(filePath);
}

void FFatHelper::listFiles(const char* path, uint8_t depth) {
    File dir = FFat.open(path);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("Failed to open directory: %s\n", path);
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        for (uint8_t i = 0; i < depth; i++) Serial.print("  ");

        if (file.isDirectory()) {
            Serial.printf("[DIR]  %s\n", file.name());

            listFiles(file.name(), depth + 1);
        } else {
            Serial.printf("FILE:  %s  SIZE: %d\n", file.name(), file.size());
        }

        file = dir.openNextFile();
    }
}

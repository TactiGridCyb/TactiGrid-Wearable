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
    } else {
        
        file.println(content);
        file.close();
    }
}

bool FFatHelper::isFileExisting(const char* filePath)
{
    return FFat.exists(filePath);
}



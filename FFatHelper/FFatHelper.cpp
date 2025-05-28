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


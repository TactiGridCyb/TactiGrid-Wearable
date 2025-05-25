
#pragma once

#include <FFat.h>

class FFatHelper
{
    public:
    static bool saveFile(const uint8_t* data, size_t len, const char* filePath);
};
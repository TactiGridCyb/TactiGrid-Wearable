#include <LilyGoLib.h>
#include <LV_Helper.h>

class LoraModule
{
private:
    inline static SX1262 loraDevice = newModule();
    inline static volatile bool receivedFlag = false;
    float freq;
public:
    LoraModule(float);

    int16_t setup();
    static ICACHE_RAM_ATTR void setFlag();
    
    ~LoraModule();
};



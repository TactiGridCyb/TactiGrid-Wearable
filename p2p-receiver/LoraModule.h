#include <LilyGoLib.h>
#include <LV_Helper.h>

class LoraModule
{
private:
    SX1262 loraDevice = newModule();

    inline static volatile bool receivedFlag = false;
    inline static volatile bool transmittedFlag = false;

    std::function<void(const String&)> onReadData;
    float freq;
public:
    LoraModule(float);

    static ICACHE_RAM_ATTR void setReceivedFlag();
    static ICACHE_RAM_ATTR void setTransmittedFlag();
    
    int16_t setup(bool);
    int16_t readData();
    int16_t sendData(const char*);
    int16_t setupListening();
    
    ~LoraModule();
};



#include <LilyGoLib.h>
#include <LV_Helper.h>


class LoraModule
{
private:
    SX1262 loraDevice = newModule();

    bool initialTransmittion = false;

    std::function<void(const String&)> onReadData;
    float freq;
public:
    LoraModule(float);
    
    int16_t setup(bool);
    int16_t sendData(const char*);
    int16_t readData();
    int16_t cleanUpTransmissions();
    int16_t setupListening();

    bool isChannelFree();
    bool canTransmit();

    void setOnReadData(std::function<void(const String&)>);
    
    ~LoraModule();
};



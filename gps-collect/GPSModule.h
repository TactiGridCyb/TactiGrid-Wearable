#include <TinyGPSPlus.h>
#include <tuple>

class GPSModule{
    private:
    TinyGPSPlus gpsInstance;
    unsigned long currentTime;
    float readInterval;
    
    public:
    GPSModule(float);

    std::tuple<float,float> readData();
};
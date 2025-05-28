#include <LVGLPage.h>
#include <LV_Helper.h>
#include <WifiModule.h>

class receiveParametersPageSoldier : public LVGLPage{
    public:
        receiveParametersPageSoldier(std::unique_ptr<WifiModule> wifiModule);

        void createPage() override;
        void destroy();

    private:
        std::unique_ptr<WifiModule> wifiModule;
        lv_obj_t* mainPage;
        lv_obj_t* statusLabels[6];

        const char* messages[6] = {
            "Received Cert", "Received CA Cert", "Received GMK", "Received Freqs", "Received Commanders Certs",
            "Received Interval"
        };

        static void onSocketOpened(lv_event_t* event);

        void updateLabel(uint8_t index);
};
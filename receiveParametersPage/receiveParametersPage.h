#include <LVGLPage.h>
#include <LV_Helper.h>
#include <WifiModule.h>

class receiveParametersPage : public LVGLPage{
    public:
        receiveParametersPage(std::shared_ptr<WifiModule> wifiModule);

        void createPage() override;
        void destroy();

    private:
        std::shared_ptr<WifiModule> wifiModule;
};
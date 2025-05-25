#include "LVGLPage.h"
#include <LilyGoLib.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>

class SoldiersGUI : public LVGLPage {
    private:


    public:
    SoldiersGUI();
    void createPage();
    void showPage();
    void destroyPage();
};
#include "LVGLPage.h"

class MainSoldiersPage : public LVGLPage {
    private:
    lv_obj_t* parent;

    public:
    MainSoldiersPage(lv_obj_t*);

    void createPage();
    void showPage();
    void destroyPage();

    static void openSendCoordsPage(lv_event_t*);
};
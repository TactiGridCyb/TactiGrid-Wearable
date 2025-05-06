#include "LVGLPage.h"

class MainSoldiersPage : public LVGLPage {
    private:
    lv_obj_t* parent;

    public:
    MainSoldiersPage(lv_obj_t*);
    void createPage();
    void showPage();
    void destroyPage();
};
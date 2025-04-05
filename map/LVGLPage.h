#include <lvgl.h>

class LVGLPage {
    public:
    virtual ~LVGLPage(){}
    virtual void createPage() = 0;
    virtual void showPage() = 0;
    virtual void destroyPage() = 0;

    static void closeAndOpenPage(LVGLPage* oldPage, LVGLPage* newPage)
    {
        oldPage->destroyPage();
        newPage->createPage();
        newPage->showPage();
    }
};


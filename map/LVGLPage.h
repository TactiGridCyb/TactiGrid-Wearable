#include <lvgl.h>

class LVGLPage {
    public:
    virtual ~LVGLPage(){}
    virtual void createPage() = 0;
    virtual void showPage() = 0;
    virtual void destroyPage() = 0;
};


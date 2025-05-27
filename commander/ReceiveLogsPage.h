#pragma once
#include "LVGLPage.h"
#include "LoraModule.h"
#include <memory>

class ReceiveLogsPage : public LVGLPage {
public:
    ReceiveLogsPage(std::shared_ptr<LoraModule> loraModule);

    void createPage() override;

private:
    std::shared_ptr<LoraModule> loraModule;
};
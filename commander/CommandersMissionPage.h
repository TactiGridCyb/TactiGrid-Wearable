#pragma once
#include "LVGLPage.h"
#include "LoraModule.h"
#include <string>
#include <memory>

class CommandersMissionPage : public LVGLPage {
public:
    CommandersMissionPage(std::shared_ptr<LoraModule> loraModule, const std::string& logFilePath);

    void createPage() override;

private:
    std::shared_ptr<LoraModule> loraModule;
    std::string logFilePath;
};
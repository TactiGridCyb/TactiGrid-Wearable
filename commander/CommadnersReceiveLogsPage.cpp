#include "ReceiveLogsPage.h"


ReceiveLogsPage::ReceiveLogsPage(std::shared_ptr<LoraModule> loraModule)
    : loraModule(std::move(loraModule)) {}

void ReceiveLogsPage::createPage() {
    // Placeholder for receive logs logic
    // Example: loraModule->setupListening();
}
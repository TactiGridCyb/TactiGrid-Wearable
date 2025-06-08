#include "FFatHelper.h"

bool FFatHelper::saveFile(const uint8_t* data, size_t len, const char* filePath)
{
    File file = FFat.open(filePath, FILE_WRITE);
    
    if (!file) {
        return false;
    }
    
    file.write(data, len);
    file.close();

    return true;
}

bool FFatHelper::deleteFile(const char* filePath)
{
    if (FFat.exists(filePath)) 
    {
        Serial.printf("File %s exists!\n", filePath);
        return FFat.remove(filePath);
    }
    else
    {
        Serial.printf("File %s doesn't exist!\n", filePath);
    }

  return false;
}

bool FFatHelper::writeToFile(const char* filePath, const char* content)
{
    File file = FFat.open(filePath, FILE_APPEND);

    if (!file) {
        Serial.println("Failed to open file for writing!");
        return false;
    } else {
        Serial.println("File existing");
        file.println(content);
        Serial.println("Closing file");
        file.close();
        return true;
    }
}

bool FFatHelper::isFileExisting(const char* filePath)
{
    return FFat.exists(filePath);
}


bool FFatHelper::appendRegularJsonObjectToFile(const char* filePath, JsonDocument& json)
{
    if (!FFat.exists(filePath)) 
    {
        Serial.println("Json must be initialized!!!");
        return false;
    }
    

    File file = FFat.open(filePath, "r+");
    if (!file) 
    {
        Serial.println("Failed to open existing JSON file for append!");
        return false;
    }

    size_t fileSize = file.size();
    if (fileSize < 2) 
    {
        Serial.println("JSON file is unexpectedly small—refusing to append.");
        file.close();
        return false;
    }
    else if (fileSize > 16384) 
    {
        Serial.println("File too large to handle in memory.");
        file.close();
        return false;
    }

    std::unique_ptr<char[]> buf(new char[fileSize + 1]);
    file.readBytes(buf.get(), fileSize);
    buf[fileSize] = '\0';
    file.close();

    DynamicJsonDocument fullDoc(16000);
    DeserializationError error = deserializeJson(fullDoc, buf.get());
    if (error) 
    {
        Serial.println("Failed to parse existing JSON file.");
        return false;
    }

    JsonArray dataArray = fullDoc["Data"].as<JsonArray>();
    if (!dataArray) 
    {
        Serial.println("Data array not found.");
        return false;
    }

    dataArray.add(json.as<JsonObject>());

    file = FFat.open(filePath, FILE_WRITE);
    if (!file) 
    {
        Serial.println("Failed to reopen file for writing.");
        return false;
    }

    serializeJson(fullDoc, file);

    file.close();
    return true;
}

bool FFatHelper::appendJSONEvent(const char* filePath, JsonDocument& event)
{
    if (!FFat.exists(filePath)) 
    {
        Serial.println("Log file does not exist.");
        return false;
    }

    File file = FFat.open(filePath, FILE_READ);
    if (!file) 
    {
        Serial.println("Failed to open log file.");
        return false;
    }

    size_t fileSize = file.size();
    if (fileSize > 16384) 
    {
        Serial.println("File too large to parse safely.");
        file.close();
        return false;
    }

    std::unique_ptr<char[]> buf(new char[fileSize + 1]);
    file.readBytes(buf.get(), fileSize);
    buf[fileSize] = '\0';
    file.close();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, buf.get());
    if (err) 
    {
        Serial.println("Failed to parse JSON log file.");
        return false;
    }

    JsonArray events = doc["Events"].as<JsonArray>();
    if (!events) {
        Serial.println("Missing 'Events' array.");
        return false;
    }

    events.add(event.as<JsonObject>());

    file = FFat.open(filePath, FILE_WRITE);
    if (!file) 
    {
        Serial.println("Failed to open file for writing.");
        return false;
    }

    serializeJson(doc, file);
    file.close();

    Serial.println("Event successfully appended.");
    return true;
}

bool FFatHelper::readFile(const char* filePath, String& outContent) {
    File file = FFat.open(filePath, FILE_READ);

    if (!file) {
        Serial.printf("Failed to open '%s' for reading\n", filePath);
        return false;
    }

    size_t fileSize = file.size();
    outContent.clear();
    outContent.reserve(fileSize);

    while (file.available()) {
        outContent += (char)file.read();
    }

    file.close();
    return true;
}

void FFatHelper::listFiles(const char* path, uint8_t depth) {
    File dir = FFat.open(path);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("Failed to open directory: %s\n", path);
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        for (uint8_t i = 0; i < depth; i++) Serial.print("  ");

        if (file.isDirectory()) {
            Serial.printf("[DIR]  %s\n", file.name());

            listFiles(file.name(), depth + 1);
        } else {
            Serial.printf("FILE:  %s  SIZE: %d\n", file.name(), file.size());
        }

        file = dir.openNextFile();
    }
}

void FFatHelper::removeFilesIncludeWords(const char* filterWord,
                                         const char* filesType)
{
    File root = FFat.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Failed to open FFat root or it’s not a directory");
        return;
    }

    std::vector<String> toDelete;
    File entry;
    while ((entry = root.openNextFile())) {
        String path = entry.name();
        if (!entry.isDirectory()
            && path.endsWith(filesType)
            && path.indexOf(filterWord) >= 0)
        {
            toDelete.push_back(path);
        }
        entry.close();
    }
    root.close();

    for (auto &rawPath : toDelete) 
    {
        String path = rawPath;
        if (!path.startsWith("/")) 
        {
            path = "/" + path;
        }

        Serial.printf("Attempting to delete: %s … ", path.c_str());
        if (!FFat.exists(path.c_str())) 
        {
            Serial.printf("⚠️  not found: %s\n", path.c_str());
        }
        else if (FFat.remove(path.c_str())) 
        {
            Serial.printf("✅ Deleted: %s\n", path.c_str());
        } else 
        {
            Serial.printf("❌ Failed to delete even though it exists: %s\n", path.c_str());
        }
    }
}

bool FFatHelper::initializeLogFile(const char* path, float intervalMS, String missionID)
{
    Serial.printf("FFatHelper::initializeLogFile %s %.2f %s\n", path, intervalMS, missionID.c_str());
    if (FFat.exists(path)) 
    {
        Serial.println("Log file already exists. Skipping initialization.");
        return false;
    }
    Serial.println("OPENING FILE");
    File file = FFat.open(path, FILE_WRITE);
    if (!file) 
    {
        Serial.println("Failed to create log file.");
        return false;
    }

    JsonDocument doc;

    doc["Interval"] = intervalMS;
    doc["Mission"] = missionID;

    doc.createNestedArray("Data");
    doc.createNestedArray("Events");

    serializeJson(doc, file);

    file.close();

    return true;
}

void FFatHelper::removeFilesStartingWith(const char* prefix) {
    File root = FFat.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("❌ Failed to open FFat root or it’s not a directory");
        return;
    }

    std::vector<String> toDelete;
    File entry;
    while ((entry = root.openNextFile())) {
        String name = entry.name();
        if (!entry.isDirectory() && name.startsWith(prefix)) {
            toDelete.push_back(name);
        }
        entry.close();
    }
    root.close();

    for (auto &rawName : toDelete) {
        String path = rawName;
        if (!path.startsWith("/")) {
            path = "/" + path;
        }

        Serial.printf("Attempting to delete: %s … ", path.c_str());
        if (!FFat.exists(path.c_str())) {
            Serial.printf("⚠️  not found: %s\n", path.c_str());
        }
        else if (FFat.remove(path.c_str())) {
            Serial.printf("✅ Deleted: %s\n", path.c_str());
        } else {
            Serial.printf("❌ Failed to delete even though it exists: %s\n",
                          path.c_str());
        }
    }
}
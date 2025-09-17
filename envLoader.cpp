#include <FFat.h>
#include <map>
#include <LilyGoLib.h>

std::map<std::string, std::string> env_map;


void load_env(const char* path = "/.env") {

        File file = FFat.open(path, FILE_READ);
        if (!file) {
                Serial.println("File Not Existing!");
                return;
        }
        Serial.println("File Existing!");

        while (file.available()) {
                String line = file.readStringUntil('\n');
                line.trim();

                if (line.length() == 0 || line.startsWith("#")) continue;

                int separator = line.indexOf('=');
                if (separator == -1) continue;

                String key = line.substring(0, separator);
                String value = line.substring(separator + 1);

                Serial.println("Found! " + key + " " + value);

                env_map[std::string(key.c_str())] = std::string(value.c_str());

        }

        file.close();
}

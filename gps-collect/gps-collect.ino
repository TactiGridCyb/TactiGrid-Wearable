#include "LilyGoLib.h"
#include "LV_Helper.h"
#include <TinyGPSPlus.h>

// The TinyGPSPlus object
TinyGPSPlus gps;
lv_obj_t *label;

void setup()
{
    Serial.begin(115200);

    watch.begin();

    beginLvglHelper();

    label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
}

void loop()
{
    uint32_t satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
    double hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 0;
    double lat = gps.location.isValid() ? gps.location.lat() : 0;
    double lng = gps.location.isValid() ? gps.location.lng() : 0;
    uint32_t age = gps.location.isValid() ? gps.location.age() : 0;
    uint16_t year = gps.date.isValid() ? gps.date.year() : 0;
    uint8_t month = gps.date.isValid() ? gps.date.month() : 0;
    uint8_t day = gps.date.isValid() ? gps.date.day() : 0;
    uint8_t hour = gps.time.isValid() ? gps.time.hour() : 0;
    uint8_t minute = gps.time.isValid() ? gps.time.minute() : 0;
    uint8_t second = gps.time.isValid() ? gps.time.second() : 0;
    double meters = gps.altitude.isValid() ? gps.altitude.meters() : 0;
    double kmph = gps.speed.isValid() ? gps.speed.kmph() : 0;

    Serial.printf(
        "Fix:%u  Sats:%u  HDOP:%.1f  Lat:%.5f  Lon:%.5f   Date:%d/%d/%d   Time:%d/%d/%d  Alt:%.2f m   Speed:%.2f  RX:%u\n",
        age, satellites, hdop, lat, lng, year, month, day, hour, minute, second, meters, kmph, gps.charsProcessed()
    );

    smartDelay(1000);
    lv_task_handler();
}

// This custom version of delay() ensures that the gps object is being "fed".
static void smartDelay(unsigned long ms)
{
    unsigned long start = millis();
    do {
        while (GPSSerial.available()) {
            int r = GPSSerial.read();
            gps.encode(r);
        }
    } while (millis() - start < ms);
}

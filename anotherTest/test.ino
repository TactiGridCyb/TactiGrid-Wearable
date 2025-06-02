#include <LilyGoLib.h>
#include <sodium.h>
#include <LV_Helper.h>

static lv_obj_t * info_box;

void restart_fadeout(lv_obj_t * info_box, uint32_t delay_ms, uint32_t fade_time_ms)
{
    lv_obj_clear_flag(info_box, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_opa(info_box, LV_OPA_COVER, 0);

    lv_obj_fade_out(info_box, fade_time_ms, delay_ms);
}

void setup() {
  Serial.begin(115200);
  watch.begin(&Serial);
  delay(500); 
  Serial.println("PRE");
  beginLvglHelper();

  Serial.println("START");
  info_box = lv_obj_create(lv_scr_act());
  lv_obj_set_size(info_box, 150, 80);
  lv_obj_center(info_box);
  Serial.println("START1");
  lv_obj_set_style_bg_color(info_box, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(info_box, LV_OPA_80, 0);
  lv_obj_set_style_radius(info_box, 6, 0);
  Serial.println("START2");
  lv_obj_t * label = lv_label_create(info_box);
  lv_obj_set_style_text_color(label, lv_color_white(), 0);
  lv_label_set_text(label, "Hello, Watch!");
  lv_obj_center(label);
  Serial.println("START3");

  lv_obj_fade_out(info_box,
                  500,
                  2000);
  Serial.println("START4");

}

time_t cur = millis();
void loop() {
  // nothing else
  delay(5);

  lv_task_handler();

  if(millis() - cur > 10000)
  {
    restart_fadeout(info_box, 2000, 500);
    cur = millis();
  }
}
#ifndef OLED_APP_H
#define OLED_APP_H
#include <stdint.h>

void UI_DrawScrollingTitle(uint8_t y, const char* str, uint32_t tick);
static void UI_OLED_ShowScrollString(int16_t x, uint8_t y, char* str);

#endif //OLED_APP_H

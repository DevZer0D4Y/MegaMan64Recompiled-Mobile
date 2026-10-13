#pragma once
#include <cstdint>
namespace mobile {
void set_touch(uint32_t buttons);
void set_stick(float x, float y);
void reset_touch();
void read_touch(uint16_t& buttons, float& x, float& y);
void set_controls_mode(int mode);
void open_menu();
constexpr int GamePadRequest = 0x50414447;
void request_gamepad();
// Up, down, left, right, confirm, back, apply.
void menu_key(int action, bool pressed);
}

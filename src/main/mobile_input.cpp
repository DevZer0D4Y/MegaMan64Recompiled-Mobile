#include "mobile_input.h"
#include <atomic>
#include <array>
#include <algorithm>
#include <cmath>
#include <SDL.h>
#ifdef __ANDROID__
#include <SDL_system.h>
#include <jni.h>
#include "ultramodern/ultramodern.hpp"
#endif
namespace { std::atomic<uint32_t> touch{0}; std::atomic<uint32_t> stick{0}; std::array<std::atomic_bool, 7> menuHeld{}; }
namespace mobile {
void set_touch(uint32_t value) { touch.store(value); }
void set_stick(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) { x = y = 0; }
    const auto sx = static_cast<int16_t>(std::clamp(x, -1.0f, 1.0f) * 32767);
    const auto sy = static_cast<int16_t>(std::clamp(y, -1.0f, 1.0f) * 32767);
    stick.store(uint32_t(uint16_t(sx)) | (uint32_t(uint16_t(sy)) << 16));
}
void menu_key(int action, bool pressed) {
    static constexpr SDL_Keycode keys[] = {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_RETURN, SDLK_ESCAPE, SDLK_f};
    static constexpr SDL_Scancode scans[] = {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_RETURN, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_F};
    if (action < 0 || action >= 7) return;
    bool wasPressed = menuHeld[action].exchange(pressed);
    if (!pressed && !wasPressed) return;
    SDL_Event event{};
    event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    event.key.repeat = pressed && wasPressed;
    event.key.keysym.sym = keys[action];
    event.key.keysym.scancode = scans[action];
    SDL_PushEvent(&event);
}
void reset_touch() {
    touch.store(0); stick.store(0);
    for (int action = 0; action < 7; ++action) menu_key(action, false);
}
void read_touch(uint16_t& buttons, float& x, float& y) {
    uint32_t value = touch.load();
    buttons |= value & 0xffff;
    uint32_t axes = stick.load();
    x += float(int16_t(axes & 0xffff)) / 32767;
    y += float(int16_t(axes >> 16)) / 32767;
    x += float((value >> 16) & 1) - float((value >> 17) & 1);
    y += float((value >> 18) & 1) - float((value >> 19) & 1);
}
void request_gamepad() {
    reset_touch();
    SDL_Event event{};event.type=SDL_USEREVENT;event.user.code=GamePadRequest;
    SDL_PushEvent(&event);
}
void open_menu() {
    for (Uint32 type : std::array<Uint32, 2>{SDL_KEYDOWN, SDL_KEYUP}) {
        SDL_Event event{}; event.type = type;
        event.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
        event.key.keysym.sym = SDLK_ESCAPE; event.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
        SDL_PushEvent(&event);
    }
}
#ifdef __ANDROID__
void set_controls_mode(int mode) {
    static int previous = -1;
    if (previous == mode) { return; }
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env) { return; }
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (!activity) { return; }
    jclass cls = env->GetObjectClass(activity);
    jmethodID method = env->GetMethodID(cls, "setControlsMode", "(I)V");
    if (method) { env->CallVoidMethod(activity, method, mode); previous=mode; reset_touch(); }
    if (env->ExceptionCheck()) { env->ExceptionClear(); previous=-1; }
    env->DeleteLocalRef(cls); env->DeleteLocalRef(activity);
}
#endif
}
#ifdef __ANDROID__
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_MegaManActivity_nativeTouch(JNIEnv*, jclass, jint mask) { mobile::set_touch(mask); }
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_MegaManActivity_nativeMenu(JNIEnv*, jclass) { mobile::open_menu(); }
#endif

#ifdef __ANDROID__
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_MegaManActivity_nativeStick(JNIEnv*, jclass, jfloat x, jfloat y) { mobile::set_stick(x, y); }
#endif

#ifdef __ANDROID__
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_MegaManActivity_nativeMenuKey(JNIEnv*, jclass, jint action, jboolean pressed) { mobile::menu_key(action, pressed); }
#endif

#ifdef __ANDROID__
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_MegaManActivity_nativeFlushSaves(JNIEnv*, jclass) {
    if (ultramodern::is_game_started()) ultramodern::flush_saves();
}
#endif

#ifdef __ANDROID__
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_MegaManActivity_nativeGamePad(JNIEnv*, jclass) { mobile::request_gamepad(); }
#endif

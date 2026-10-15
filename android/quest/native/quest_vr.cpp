#include <jni.h>
#include <EGL/egl.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <android/log.h>
#include <dlfcn.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
JavaVM* vm = nullptr;
jobject activity = nullptr;
std::thread worker;
std::atomic<bool> stopping{false};
std::atomic<int> mode{2};
using Gate = void (*)(bool);
std::atomic<Gate> gate{nullptr};
std::atomic<bool> xrVisible{false};
using Touch = void (*)(JNIEnv*, jclass, jint);
using Stick = void (*)(JNIEnv*, jclass, jfloat, jfloat);
using MenuKey = void (*)(JNIEnv*, jclass, jint, jboolean);
using Menu = void (*)(JNIEnv*, jclass);
Touch touch = nullptr;
Stick stick = nullptr;
MenuKey menuKey = nullptr;
Menu menu = nullptr;
void check(XrResult result, const char* operation) {
    if (XR_FAILED(result)) throw std::runtime_error(std::string(operation) + " failed (OpenXR " + std::to_string(result) + ")");
}
void surfaceCallback(JNIEnv* env, jobject surface) {
    jclass cls = env->GetObjectClass(activity);
    env->CallVoidMethod(activity, env->GetMethodID(cls, "onQuestSurface", "(Landroid/view/Surface;)V"), surface);
    env->DeleteLocalRef(cls);
}
void errorCallback(JNIEnv* env, const std::string& error) {
    __android_log_print(ANDROID_LOG_ERROR, "MegaManQuest", "%s", error.c_str());
    jclass cls = env->GetObjectClass(activity);
    jstring message = env->NewStringUTF(error.c_str());
    env->CallVoidMethod(activity, env->GetMethodID(cls, "onQuestError", "(Ljava/lang/String;)V"), message);
    env->DeleteLocalRef(message); env->DeleteLocalRef(cls);
}
XrPath path(XrInstance instance, const char* text) {
    XrPath value; check(xrStringToPath(instance, text, &value), "xrStringToPath"); return value;
}
struct Input {
    XrActionSet set = XR_NULL_HANDLE;
    XrAction leftStick, rightStick, leftTrigger, rightTrigger, leftGrip, rightGrip;
    XrAction a, b, x, y, menuButton, recenter, start;
    std::array<bool, 7> held{};
    std::array<std::chrono::steady_clock::time_point, 7> repeatAt{};
    bool menuHeld = false, recenterHeld = false;
    int previousMode = -1;
    XrAction create(XrActionType type, const char* name) {
        XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO}; info.actionType = type;
        std::strncpy(info.actionName, name, XR_MAX_ACTION_NAME_SIZE - 1);
        std::strncpy(info.localizedActionName, name, XR_MAX_LOCALIZED_ACTION_NAME_SIZE - 1);
        XrAction result; check(xrCreateAction(set, &info, &result), "xrCreateAction"); return result;
    }
    void initialize(XrInstance instance, XrSession session) {
        XrActionSetCreateInfo info{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::strcpy(info.actionSetName, "gamepad"); std::strcpy(info.localizedActionSetName, "Game controls");
        check(xrCreateActionSet(instance, &info, &set), "xrCreateActionSet");
        leftStick = create(XR_ACTION_TYPE_VECTOR2F_INPUT, "movement");
        rightStick = create(XR_ACTION_TYPE_VECTOR2F_INPUT, "c_buttons");
        leftTrigger = create(XR_ACTION_TYPE_FLOAT_INPUT, "left_trigger");
        rightTrigger = create(XR_ACTION_TYPE_FLOAT_INPUT, "right_trigger");
        leftGrip = create(XR_ACTION_TYPE_FLOAT_INPUT, "left_grip");
        rightGrip = create(XR_ACTION_TYPE_FLOAT_INPUT, "right_grip");
        a = create(XR_ACTION_TYPE_BOOLEAN_INPUT, "a"); b = create(XR_ACTION_TYPE_BOOLEAN_INPUT, "b");
        x = create(XR_ACTION_TYPE_BOOLEAN_INPUT, "x"); y = create(XR_ACTION_TYPE_BOOLEAN_INPUT, "y");
        menuButton = create(XR_ACTION_TYPE_BOOLEAN_INPUT, "options");
        recenter = create(XR_ACTION_TYPE_BOOLEAN_INPUT, "recenter"); start = create(XR_ACTION_TYPE_BOOLEAN_INPUT, "start");
        std::vector<XrActionSuggestedBinding> bindings;
        auto bind = [&](XrAction action, const char* text) { bindings.push_back({action, path(instance, text)}); };
        bind(leftStick, "/user/hand/left/input/thumbstick"); bind(rightStick, "/user/hand/right/input/thumbstick");
        bind(leftTrigger, "/user/hand/left/input/trigger/value"); bind(rightTrigger, "/user/hand/right/input/trigger/value");
        bind(leftGrip, "/user/hand/left/input/squeeze/value"); bind(rightGrip, "/user/hand/right/input/squeeze/value");
        bind(a, "/user/hand/right/input/a/click"); bind(b, "/user/hand/right/input/b/click");
        bind(x, "/user/hand/left/input/x/click"); bind(y, "/user/hand/left/input/y/click");
        bind(menuButton, "/user/hand/left/input/menu/click");
        bind(recenter, "/user/hand/left/input/thumbstick/click"); bind(start, "/user/hand/right/input/thumbstick/click");
        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile = path(instance, "/interaction_profiles/oculus/touch_controller");
        suggested.countSuggestedBindings = bindings.size(); suggested.suggestedBindings = bindings.data();
        check(xrSuggestInteractionProfileBindings(instance, &suggested), "Touch controller bindings");
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO}; attach.countActionSets = 1; attach.actionSets = &set;
        check(xrAttachSessionActionSets(session, &attach), "xrAttachSessionActionSets");
    }
    bool boolean(XrSession session, XrAction action) {
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO}; info.action = action;
        XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
        check(xrGetActionStateBoolean(session, &info, &state), "xrGetActionStateBoolean"); return state.isActive && state.currentState;
    }
    float scalar(XrSession session, XrAction action) {
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO}; info.action = action;
        XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
        check(xrGetActionStateFloat(session, &info, &state), "xrGetActionStateFloat"); return state.isActive ? state.currentState : 0;
    }
    XrVector2f vector(XrSession session, XrAction action) {
        XrActionStateGetInfo info{XR_TYPE_ACTION_STATE_GET_INFO}; info.action = action;
        XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
        check(xrGetActionStateVector2f(session, &info, &state), "xrGetActionStateVector2f"); return state.isActive ? state.currentState : XrVector2f{};
    }
    void clear(JNIEnv* env) {
        touch(env, nullptr, 0); stick(env, nullptr, 0, 0);
        for (int i = 0; i < 7; ++i) { if (held[i]) menuKey(env, nullptr, i, false); held[i] = false; }
        menuHeld = recenterHeld = false;
    }
    bool update(JNIEnv* env, XrSession session) {
        XrActiveActionSet active{set, XR_NULL_PATH};
        XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO}; sync.countActiveActionSets = 1; sync.activeActionSets = &active;
        XrResult result = xrSyncActions(session, &sync);
        if (result == XR_SESSION_NOT_FOCUSED) { clear(env); return false; }
        check(result, "xrSyncActions");
        int currentMode = mode.load();
        if (previousMode != currentMode) { clear(env); previousMode = currentMode; }
        auto left = vector(session, leftStick); auto right = vector(session, rightStick);
        bool accept = boolean(session, a), back = boolean(session, b), extra = boolean(session, x), apply = boolean(session, y);
        bool options = boolean(session, menuButton), center = boolean(session, recenter), startDown = boolean(session, start);
        bool centerPressed = center && !recenterHeld; recenterHeld = center;
        if (options && !menuHeld) menu(env, nullptr); menuHeld = options;
        if (currentMode == 1) {
            float radius = std::hypot(left.x, left.y);
            float magnitude = std::clamp((radius - .18f) / .82f, 0.f, 1.f);
            stick(env, nullptr, radius > 0 ? left.x / radius * magnitude : 0, radius > 0 ? left.y / radius * magnitude : 0);
            uint32_t buttons = (accept ? 0x8000 : 0) | (back ? 0x4000 : 0) | ((apply || startDown) ? 0x1000 : 0) | (extra ? 8 : 0);
            if (scalar(session, leftTrigger) > .55f) buttons |= 0x2000;
            if (scalar(session, rightTrigger) > .55f || scalar(session, rightGrip) > .55f) buttons |= 0x10;
            if (scalar(session, leftGrip) > .55f) buttons |= 0x20;
            if (right.x > .55f) buttons |= 1; if (right.x < -.55f) buttons |= 2;
            if (right.y > .55f) buttons |= 8; if (right.y < -.55f) buttons |= 4;
            touch(env, nullptr, buttons);
        } else {
            std::array<bool, 7> pressed{left.y > .55f, left.y < -.55f, left.x < -.55f, left.x > .55f, accept, back, apply};
            auto now = std::chrono::steady_clock::now();
            for (int i = 0; i < 7; ++i) {
                if (pressed[i] != held[i]) {
                    menuKey(env, nullptr, i, pressed[i]); held[i] = pressed[i]; repeatAt[i] = now + std::chrono::milliseconds(350);
                } else if (i < 4 && pressed[i] && now >= repeatAt[i]) {
                    menuKey(env, nullptr, i, true); repeatAt[i] = now + std::chrono::milliseconds(100);
                }
            }
        }
        return centerPressed;
    }
};
void run() {
    JNIEnv* env = nullptr;
    if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    XrInstance instance = XR_NULL_HANDLE; XrSession session = XR_NULL_HANDLE;
    XrSpace local = XR_NULL_HANDLE, view = XR_NULL_HANDLE; XrSwapchain swapchain = XR_NULL_HANDLE;
    jobject surface = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY; EGLContext context = EGL_NO_CONTEXT; EGLSurface buffer = EGL_NO_SURFACE;
    Input input;
    bool running = false, visible = false, centered = false;
    try {
        void* main = dlopen("libmain.so", RTLD_NOW);
        if (!main) throw std::runtime_error("Game library unavailable");
        gate.store(reinterpret_cast<Gate>(dlsym(main, "megaman64_quest_surface_enabled")));
        touch = reinterpret_cast<Touch>(dlsym(main, "Java_com_devz_megaman64_MegaManActivity_nativeTouch"));
        stick = reinterpret_cast<Stick>(dlsym(main, "Java_com_devz_megaman64_MegaManActivity_nativeStick"));
        menuKey = reinterpret_cast<MenuKey>(dlsym(main, "Java_com_devz_megaman64_MegaManActivity_nativeMenuKey"));
        menu = reinterpret_cast<Menu>(dlsym(main, "Java_com_devz_megaman64_MegaManActivity_nativeMenu"));
        if (!gate.load() || !touch || !stick || !menuKey || !menu) throw std::runtime_error("APK game/VR libraries do not match");
        gate.load()(false);
        PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
        check(xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", reinterpret_cast<PFN_xrVoidFunction*>(&initializeLoader)), "OpenXR loader initialization");
        XrLoaderInitInfoAndroidKHR loader{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR}; loader.applicationVM = vm; loader.applicationContext = activity;
        check(initializeLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loader)), "xrInitializeLoaderKHR");
        const char* extensions[]{XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME, XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME, XR_KHR_ANDROID_SURFACE_SWAPCHAIN_EXTENSION_NAME};
        uint32_t count; check(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr), "Extension count");
        std::vector<XrExtensionProperties> supported(count, {XR_TYPE_EXTENSION_PROPERTIES});
        check(xrEnumerateInstanceExtensionProperties(nullptr, count, &count, supported.data()), "Extension list");
        for (auto required : extensions) {
            if (std::none_of(supported.begin(), supported.end(), [&](auto& value) { return std::strcmp(value.extensionName, required) == 0; }))
                throw std::runtime_error(std::string("Quest runtime is missing ") + required + ". Update Horizon OS.");
        }
        XrInstanceCreateInfoAndroidKHR android{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR}; android.applicationVM = vm; android.applicationActivity = activity;
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO}; create.next = &android;
        std::strcpy(create.applicationInfo.applicationName, "MegaMan64 Quest"); create.applicationInfo.applicationVersion = 1; create.applicationInfo.apiVersion = XR_API_VERSION_1_0;
        create.enabledExtensionCount = 3; create.enabledExtensionNames = extensions;
        check(xrCreateInstance(&create, &instance), "xrCreateInstance");
        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO}; systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrSystemId system; check(xrGetSystem(instance, &systemInfo, &system), "xrGetSystem");
        PFN_xrGetOpenGLESGraphicsRequirementsKHR graphicsRequirements;
        check(xrGetInstanceProcAddr(instance, "xrGetOpenGLESGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&graphicsRequirements)), "Graphics requirements function");
        XrGraphicsRequirementsOpenGLESKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
        check(graphicsRequirements(instance, system, &requirements), "OpenGL ES requirements");
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (!eglInitialize(display, nullptr, nullptr)) throw std::runtime_error("EGL initialization failed");
        EGLint attributes[]{EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
        EGLConfig config; EGLint configCount;
        if (!eglChooseConfig(display, attributes, &config, 1, &configCount) || !configCount) throw std::runtime_error("No EGL ES3 configuration");
        EGLint contextAttributes[]{EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE}; context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttributes);
        EGLint bufferAttributes[]{EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE}; buffer = eglCreatePbufferSurface(display, config, bufferAttributes);
        if (!eglMakeCurrent(display, buffer, buffer, context)) throw std::runtime_error("EGL context activation failed");
        XrGraphicsBindingOpenGLESAndroidKHR graphics{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR}; graphics.display = display; graphics.config = config; graphics.context = context;
        XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO}; sessionInfo.next = &graphics; sessionInfo.systemId = system;
        check(xrCreateSession(instance, &sessionInfo, &session), "xrCreateSession");
        XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO}; space.poseInReferenceSpace.orientation.w = 1; space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        check(xrCreateReferenceSpace(session, &space, &local), "LOCAL space"); space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        check(xrCreateReferenceSpace(session, &space, &view), "VIEW space");
        PFN_xrCreateSwapchainAndroidSurfaceKHR createSurface;
        check(xrGetInstanceProcAddr(instance, "xrCreateSwapchainAndroidSurfaceKHR", reinterpret_cast<PFN_xrVoidFunction*>(&createSurface)), "Surface function");
        XrSwapchainCreateInfo swapInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO}; swapInfo.width = 1600; swapInfo.height = 900;
        swapInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        jobject returnedSurface = nullptr;
        check(createSurface(session, &swapInfo, &swapchain, &returnedSurface), "Android Surface swapchain");
        // Keep an application-owned reference regardless of the runtime's JNI reference type.
        surface = env->NewGlobalRef(returnedSurface);
        if (env->GetObjectRefType(returnedSurface) == JNILocalRefType) env->DeleteLocalRef(returnedSurface);
        if (!surface) throw std::runtime_error("Unable to retain the OpenXR Surface");
        input.initialize(instance, session);
        XrPosef screenPose{}; screenPose.orientation.w = 1; screenPose.position.z = -3;
        while (!stopping.load()) {
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            XrResult result;
            while ((result = xrPollEvent(instance, &event)) == XR_SUCCESS) {
                if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                    auto state = reinterpret_cast<XrEventDataSessionStateChanged*>(&event)->state;
                    if (state == XR_SESSION_STATE_READY && !running) {
                        XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO}; begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        check(xrBeginSession(session, &begin), "xrBeginSession"); running = true;
                    }
                    bool nowVisible = state == XR_SESSION_STATE_VISIBLE || state == XR_SESSION_STATE_FOCUSED;
                    if (visible != nowVisible) {
                        visible = nowVisible; xrVisible.store(visible);
                        if (!visible) { gate.load()(false); input.clear(env); }
                        surfaceCallback(env, visible ? surface : nullptr);
                    }
                    if (state == XR_SESSION_STATE_STOPPING && running) { gate.load()(false); check(xrEndSession(session), "xrEndSession"); running = false; }
                    if (state == XR_SESSION_STATE_EXITING || state == XR_SESSION_STATE_LOSS_PENDING) stopping.store(true);
                } else if (event.type == XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) centered = false;
                else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) stopping.store(true);
                event = {XR_TYPE_EVENT_DATA_BUFFER};
            }
            if (result != XR_EVENT_UNAVAILABLE) check(result, "xrPollEvent");
            if (!running || stopping.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO}; XrFrameState frame{XR_TYPE_FRAME_STATE}; check(xrWaitFrame(session, &wait, &frame), "xrWaitFrame");
            XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO}; check(xrBeginFrame(session, &begin), "xrBeginFrame");
            bool recenter = visible && input.update(env, session);
            if (visible && (!centered || recenter)) {
                XrSpaceLocation head{XR_TYPE_SPACE_LOCATION}; check(xrLocateSpace(view, local, frame.predictedDisplayTime, &head), "Head pose");
                constexpr XrSpaceLocationFlags valid = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
                if ((head.locationFlags & valid) == valid) {
                    auto q = head.pose.orientation;
                    float yaw = std::atan2(2 * (q.w * q.y + q.x * q.z), 1 - 2 * (q.y * q.y + q.x * q.x));
                    screenPose.orientation = {0, std::sin(yaw / 2), 0, std::cos(yaw / 2)};
                    screenPose.position = {head.pose.position.x - 3 * std::sin(yaw), head.pose.position.y, head.pose.position.z - 3 * std::cos(yaw)}; centered = true;
                }
            }
            XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD}; quad.space = local; quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            quad.subImage.swapchain = swapchain; quad.subImage.imageRect.extent = {1600, 900}; quad.pose = screenPose; quad.size = {3.2f, 1.8f};
            const XrCompositionLayerBaseHeader* layers[]{reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad)};
            XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO}; end.displayTime = frame.predictedDisplayTime; end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
            end.layerCount = visible && frame.shouldRender && centered ? 1 : 0; end.layers = end.layerCount ? layers : nullptr;
            check(xrEndFrame(session, &end), "xrEndFrame");
        }
    } catch (const std::exception& error) { errorCallback(env, error.what()); }
    xrVisible.store(false);
    if (gate.load()) gate.load()(false);
    if (touch && stick && menuKey) input.clear(env);
    surfaceCallback(env, nullptr);
    if (swapchain) xrDestroySwapchain(swapchain);
    if (view) xrDestroySpace(view); if (local) xrDestroySpace(local);
    if (session) xrDestroySession(session); if (input.set) xrDestroyActionSet(input.set); if (instance) xrDestroyInstance(instance);
    if (surface) env->DeleteGlobalRef(surface);
    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (buffer != EGL_NO_SURFACE) eglDestroySurface(display, buffer);
        if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
        eglTerminate(display);
    }
    vm->DetachCurrentThread();
}
}
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_QuestActivity_nativeStart(JNIEnv* env, jclass, jobject app) {
    if (worker.joinable()) return;
    env->GetJavaVM(&vm); activity = env->NewGlobalRef(app); stopping.store(false); worker = std::thread(run);
}
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_QuestActivity_nativeStop(JNIEnv* env, jclass) {
    stopping.store(true); if (worker.joinable()) worker.join(); if (activity) env->DeleteGlobalRef(activity); activity = nullptr;
}
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_QuestActivity_nativeMode(JNIEnv*, jclass, jint value) { mode.store(value); }
extern "C" JNIEXPORT void JNICALL Java_com_devz_megaman64_QuestActivity_nativeSurfaceEnabled(JNIEnv*, jclass, jboolean enabled) { if (gate.load()) gate.load()(enabled && xrVisible.load()); }

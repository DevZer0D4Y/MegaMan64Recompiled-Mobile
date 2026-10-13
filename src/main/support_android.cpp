#include "zelda_support.h"
#include <SDL.h>
#include <SDL_system.h>
#include <jni.h>
#include <mutex>
#include <deque>

namespace {
std::mutex dialog_mutex;
std::function<void(bool, const std::list<std::filesystem::path>&)> pending_callback;
std::deque<std::function<void()>> ready_callbacks;
void complete_dialog(std::list<std::filesystem::path> paths) {
    std::lock_guard lock(dialog_mutex);
    if (!pending_callback) { return; }
    auto callback = std::move(pending_callback);
    pending_callback = {};
    ready_callbacks.push_back([callback, paths = std::move(paths)] { callback(!paths.empty(), paths); });
}
void select_documents(bool multiple, std::function<void(bool, const std::list<std::filesystem::path>&)> callback) {
    {
        std::lock_guard lock(dialog_mutex);
        if (pending_callback) {
            ready_callbacks.push_back([callback] { callback(false, {}); });
            return;
        }
        pending_callback = std::move(callback);
    }
    auto* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    if (!env) { complete_dialog({}); return; }
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    jclass clazz = activity ? env->GetObjectClass(activity) : nullptr;
    jmethodID method = clazz ? env->GetMethodID(clazz, "openDocuments", "(Z)V") : nullptr;
    if (method) { env->CallVoidMethod(activity, method, static_cast<jboolean>(multiple)); }
    bool failed = !method || env->ExceptionCheck();
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    if (clazz) { env->DeleteLocalRef(clazz); }
    if (activity) { env->DeleteLocalRef(activity); }
    if (failed) { complete_dialog({}); }
}
}

extern "C" JNIEXPORT void JNICALL
Java_com_devz_megaman64_MegaManActivity_nativeDocumentsSelected(
    JNIEnv* env, jclass, jobjectArray paths) {
    std::list<std::filesystem::path> selected;
    if (paths) {
        for (jsize i = 0; i < env->GetArrayLength(paths); ++i) {
            auto value = static_cast<jstring>(env->GetObjectArrayElement(paths, i));
            if (!value) { continue; }
            const char* path = env->GetStringUTFChars(value, nullptr);
            if (path) {
                selected.emplace_back(path);
                env->ReleaseStringUTFChars(value, path);
            }
            env->DeleteLocalRef(value);
        }
    }
    complete_dialog(std::move(selected));
}

namespace zelda64 {
void process_android_callbacks() {
    std::deque<std::function<void()>> ready;
    { std::lock_guard lock(dialog_mutex); ready.swap(ready_callbacks); }
    for (auto& callback : ready) { callback(); }
}
bool initialize_android_storage() {
    const char* storage = SDL_AndroidGetInternalStoragePath();
    if (!storage) { return false; }
    std::error_code error;
    std::filesystem::current_path(storage, error);
    return !error;
}
std::filesystem::path get_rom_drop_directory() {
    const char* storage = SDL_AndroidGetExternalStoragePath();
    return storage ? std::filesystem::path{storage} : std::filesystem::path{};
}
std::filesystem::path get_program_path() {
    const char* storage = SDL_AndroidGetInternalStoragePath();
    return storage ? std::filesystem::path{storage} : std::filesystem::path{};
}
std::filesystem::path get_asset_path(const char* asset) {
    return get_program_path() / "assets" / asset;
}
void open_file_dialog(std::function<void(bool, const std::filesystem::path&)> callback) {
    select_documents(false, [callback](bool success, const std::list<std::filesystem::path>& paths) {
        callback(success, paths.empty() ? std::filesystem::path{} : paths.front());
    });
}
void open_file_dialog_multiple(std::function<void(bool, const std::list<std::filesystem::path>&)> callback) {
    select_documents(true, std::move(callback));
}
void show_error_message_box(const char* title, const char* message) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message, nullptr);
}
}

#include "zelda_support.h"
#include <SDL.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <mutex>
#include <deque>
#include <utility>
#include <stdexcept>

namespace {
std::mutex callback_mutex;
std::deque<std::function<void()>> callbacks;
bool picker_open = false;
void deliver(std::function<void()> callback) {
    std::lock_guard lock(callback_mutex);
    callbacks.push_back(std::move(callback));
}
UIViewController* active_controller() {
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:UIWindowScene.class]) {
            for (UIWindow* window in ((UIWindowScene*)scene).windows) {
                if (window.isKeyWindow) {
                    UIViewController* controller = window.rootViewController;
                    while (controller.presentedViewController) { controller = controller.presentedViewController; }
                    return controller;
                }
            }
        }
    }
    // SDL2 also supports the UIApplication window lifecycle without scenes.
    UIWindow* window = UIApplication.sharedApplication.keyWindow;
    if (!window) { window = UIApplication.sharedApplication.windows.firstObject; }
    UIViewController* controller = window.rootViewController;
    while (controller.presentedViewController) { controller = controller.presentedViewController; }
    return controller;
}
}

@interface MMDocumentDelegate : NSObject <UIDocumentPickerDelegate>
@property(nonatomic) std::function<void(bool, const std::list<std::filesystem::path>&)> completion;
@end
static MMDocumentDelegate* picker_delegate;

@implementation MMDocumentDelegate
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    std::list<std::filesystem::path> paths;
    NSString* directory = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    NSError* error = nil;
    [NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:&error];
    bool success = !error;
    for (NSURL* url in urls) {
        BOOL access = [url startAccessingSecurityScopedResource];
        NSString* destination = [directory stringByAppendingPathComponent:url.lastPathComponent];
        if (![NSFileManager.defaultManager copyItemAtURL:url toURL:[NSURL fileURLWithPath:destination] error:&error]) {
            success = false;
        } else { paths.emplace_back(destination.UTF8String); }
        if (access) { [url stopAccessingSecurityScopedResource]; }
    }
    auto callback = self.completion;
    picker_open = false;
    deliver([callback, success, paths] { callback(success && !paths.empty(), paths); });
}
- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    auto callback = self.completion;
    picker_open = false;
    deliver([callback] { callback(false, {}); });
}
@end

namespace zelda64 {
void dispatch_on_ui_thread(std::function<void()> callback) {
    dispatch_async(dispatch_get_main_queue(), ^{ callback(); });
}
void process_ios_callbacks() {
    std::deque<std::function<void()>> ready;
    { std::lock_guard lock(callback_mutex); ready.swap(callbacks); }
    for (auto& callback : ready) { callback(); }
}
std::optional<std::filesystem::path> get_application_support_directory() {
    NSString* path = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject;
    if (!path) { return std::nullopt; }
    return std::filesystem::path{path.UTF8String};
}
std::filesystem::path get_bundle_resource_directory() {
    return std::filesystem::path{NSBundle.mainBundle.resourcePath.UTF8String};
}
std::filesystem::path get_bundle_directory() {
    return std::filesystem::path{NSBundle.mainBundle.bundlePath.UTF8String};
}
std::filesystem::path get_program_path() { return get_bundle_resource_directory(); }
std::filesystem::path get_asset_path(const char* name) { return get_program_path() / "assets" / name; }
std::filesystem::path get_rom_drop_directory() {
    NSString* path = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    return path ? std::filesystem::path{path.UTF8String} : std::filesystem::path{};
}
void initialize_ios_storage() {
    NSString* path = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    std::error_code error;
    std::filesystem::current_path(path.UTF8String, error);
    if (error) { throw std::runtime_error("Unable to initialize app documents directory"); }
}
void open_file_dialog_multiple(std::function<void(bool, const std::list<std::filesystem::path>&)> callback) {
    dispatch_on_ui_thread([callback] {
        UIViewController* controller = active_controller();
        if (picker_open || !controller) { deliver([callback] { callback(false, {}); }); return; }
        picker_open = true;
        picker_delegate = [MMDocumentDelegate new];
        picker_delegate.completion = callback;
        UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData] asCopy:YES];
        picker.allowsMultipleSelection = YES;
        picker.delegate = picker_delegate;
        [controller presentViewController:picker animated:YES completion:nil];
    });
}
void open_file_dialog(std::function<void(bool, const std::filesystem::path&)> callback) {
    open_file_dialog_multiple([callback](bool success, const std::list<std::filesystem::path>& paths) {
        callback(success, paths.empty() ? std::filesystem::path{} : paths.front());
    });
}
void show_error_message_box(const char* title, const char* message) {
    std::string title_copy{title}, message_copy{message};
    dispatch_on_ui_thread([title_copy, message_copy] {
        UIAlertController* alert = [UIAlertController alertControllerWithTitle:@(title_copy.c_str()) message:@(message_copy.c_str()) preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
        [active_controller() presentViewController:alert animated:YES completion:nil];
    });
}
}

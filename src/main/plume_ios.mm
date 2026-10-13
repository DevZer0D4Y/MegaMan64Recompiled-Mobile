#include "plume_apple.h"
#import <UIKit/UIKit.h>
#include <unordered_map>
#include <cstdint>
#import <QuartzCore/CAMetalLayer.h>

namespace {
std::mutex lifetime_mutex;
std::unordered_map<const void*, uint64_t> live_windows;
uint64_t next_lifetime = 0;
}
namespace plume {
RenderDeviceVendor getRenderDeviceVendor(uint64_t) { return RenderDeviceVendor::APPLE; }
CocoaWindow::CocoaWindow(void* window) : windowHandle(window), cachedRefreshRate(60) {
    { std::lock_guard lock(lifetime_mutex); live_windows[this] = ++next_lifetime; }
    cachedAttributes = {0, 0, 0, 0};
    updateWindowAttributesInternal();
    updateRefreshRateInternal();
}
CocoaWindow::~CocoaWindow() { std::lock_guard lock(lifetime_mutex); live_windows.erase(this); }
void CocoaWindow::updateWindowAttributesInternal(bool) {
    // Avoid dispatch_sync: the main thread can be waiting for the renderer.
    uint64_t lifetime;
    { std::lock_guard lock(lifetime_mutex); lifetime = live_windows.at(this); }
    dispatch_async(dispatch_get_main_queue(), ^{
        std::lock_guard lifetime_lock(lifetime_mutex);
        auto found = live_windows.find(this);
        if (found == live_windows.end() || found->second != lifetime) { return; }
        UIWindow* window = (__bridge UIWindow*)windowHandle;
        CGRect bounds = window.bounds;
        std::lock_guard lock(attributesMutex);
        cachedAttributes = {0, 0, (int)(bounds.size.width * window.screen.scale), (int)(bounds.size.height * window.screen.scale)};
    });
}
void CocoaWindow::updateRefreshRateInternal(bool) {
    uint64_t lifetime;
    { std::lock_guard lock(lifetime_mutex); lifetime = live_windows.at(this); }
    dispatch_async(dispatch_get_main_queue(), ^{
        std::lock_guard lifetime_lock(lifetime_mutex);
        auto found = live_windows.find(this);
        if (found == live_windows.end() || found->second != lifetime) { return; }
        UIWindow* window = (__bridge UIWindow*)windowHandle;
        cachedRefreshRate.store((int)window.screen.maximumFramesPerSecond);
    });
}
void CocoaWindow::getWindowAttributes(CocoaWindowAttributes* attributes) const {
    {
        std::lock_guard lock(attributesMutex);
        *attributes = cachedAttributes;
    }
    const_cast<CocoaWindow*>(this)->updateWindowAttributesInternal();
}
int CocoaWindow::getRefreshRate() const {
    const_cast<CocoaWindow*>(this)->updateRefreshRateInternal();
    return cachedRefreshRate.load();
}
void CocoaWindow::toggleFullscreen() {}
}

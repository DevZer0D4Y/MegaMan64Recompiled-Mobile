// The UIScene lifecycle on iOS. Newer iOS releases only launch apps that
// adopt it, and SDL2 does not: Info.plist declares one scene with
// MMSceneDelegate, and every UIWindow SDL makes (the game's, message boxes)
// is attached to that scene as it is created, or it would never be shown.
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#include <cstdio>
#include "ultramodern/ultramodern.hpp"

static UIWindowScene* mm_scene = nil;
// A black window from the moment the scene connects, so iOS can take down the
// launch screen before SDL makes its first window.
static UIWindow* mm_placeholder = nil;

// SDL2 predates UIScene. Restore its window using the scene's coordinates,
// rather than screen/status-bar coordinates changed by system overlays.
static void mm_restore_display(UIWindowScene* scene, const char* reason) {
    if (!scene || scene.activationState != UISceneActivationStateForegroundActive) return;
    CGRect bounds = scene.coordinateSpace.bounds;
    fprintf(stderr, "iOS display (%s): scene %.0f x %.0f, orientation %ld\n",
        reason, bounds.size.width, bounds.size.height, (long)scene.interfaceOrientation);
    for (UIWindow* window in scene.windows) {
        if (window.hidden || window == mm_placeholder) continue;
        UIViewController* controller = window.rootViewController;
        [controller setNeedsUpdateOfSupportedInterfaceOrientations];
        if (UIInterfaceOrientationIsLandscape(scene.interfaceOrientation) && bounds.size.width > bounds.size.height) {
            window.frame = bounds;
            controller.view.frame = window.bounds;
            [window setNeedsLayout];
            [window layoutIfNeeded];
            [controller.view setNeedsLayout];
            [controller.view layoutIfNeeded];
            fprintf(stderr, "iOS display: restored view %.0f x %.0f\n",
                controller.view.bounds.size.width, controller.view.bounds.size.height);
        }
    }
    if (!UIInterfaceOrientationIsLandscape(scene.interfaceOrientation)) {
        UIWindowSceneGeometryPreferencesIOS* preferences = [[UIWindowSceneGeometryPreferencesIOS alloc]
            initWithInterfaceOrientations:UIInterfaceOrientationMaskLandscape];
        [scene requestGeometryUpdateWithPreferences:preferences errorHandler:^(NSError* error) {
            fprintf(stderr, "iOS display: landscape restore failed: %s\n", error.localizedDescription.UTF8String);
        }];
    }
}

@interface MMLaunchController : UIViewController
@end
@implementation MMLaunchController
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
@end


static void mm_attach(UIWindow* window) {
    if (window == nil || mm_scene == nil) return;
    if (window.windowScene != mm_scene) {
        window.windowScene = mm_scene;
        window.frame = mm_scene.coordinateSpace.bounds;
    }
    if (window != mm_placeholder && mm_placeholder != nil) {
        mm_placeholder.hidden = YES;
        mm_placeholder = nil;
    }
}

@interface UIWindow (MMScene)
- (instancetype)mm_initWithFrame:(CGRect)frame __attribute__((objc_method_family(init)));
@end

@implementation UIWindow (MMScene)
+ (void)load {
    Class type = [UIWindow class];
    SEL original_selector = @selector(initWithFrame:), replacement_selector = @selector(mm_initWithFrame:);
    Method original = class_getInstanceMethod(type, original_selector);
    Method replacement = class_getInstanceMethod(type, replacement_selector);
    // UIWindow may only inherit initWithFrame: from UIView: then add it to UIWindow alone.
    if (class_addMethod(type, original_selector, method_getImplementation(replacement), method_getTypeEncoding(replacement)))
        class_replaceMethod(type, replacement_selector, method_getImplementation(original), method_getTypeEncoding(original));
    else
        method_exchangeImplementations(original, replacement);
}

- (instancetype)mm_initWithFrame:(CGRect)frame {
    self = [self mm_initWithFrame:frame];  // the original, as the two are swapped
    if (self != nil && self.windowScene == nil && mm_scene != nil) self.windowScene = mm_scene;
    return self;
}
@end

// Named in Info.plist (UIApplicationSceneManifest).
@interface MMSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic, strong) UIWindow* window;
@end

@implementation MMSceneDelegate
- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)options {
    if (![scene isKindOfClass:[UIWindowScene class]]) return;
    mm_scene = (UIWindowScene*)scene;
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(screenshotTaken:)
        name:UIApplicationUserDidTakeScreenshotNotification object:nil];
    // Windows SDL made before the scene connected join it now.
    for (UIWindow* window in UIApplication.sharedApplication.windows) {
        if (window.windowScene == nil) mm_attach(window);
        if (!window.hidden) [window makeKeyAndVisible];
    }
    if (mm_scene.windows.count == 0) {
        UIViewController* controller = [[MMLaunchController alloc] init];
        controller.view.backgroundColor = UIColor.blackColor;
        mm_placeholder = [[UIWindow alloc] initWithWindowScene:mm_scene];
        mm_placeholder.backgroundColor = UIColor.blackColor;
        mm_placeholder.rootViewController = controller;
        [mm_placeholder makeKeyAndVisible];
    }
    self.window = mm_scene.windows.firstObject;
    [CATransaction flush];
}

- (void)sceneDidEnterBackground:(UIScene*)scene {
    if (ultramodern::is_game_started()) ultramodern::flush_saves();
}

- (void)sceneDidBecomeActive:(UIScene*)scene {
    dispatch_async(dispatch_get_main_queue(), ^{
        mm_restore_display((UIWindowScene*)scene, "became active");
    });
}

- (void)windowScene:(UIWindowScene*)scene didUpdateCoordinateSpace:(id<UICoordinateSpace>)previousCoordinateSpace
    interfaceOrientation:(UIInterfaceOrientation)previousInterfaceOrientation traitCollection:(UITraitCollection*)previousTraitCollection {
    dispatch_async(dispatch_get_main_queue(), ^{
        mm_restore_display(scene, "scene geometry changed");
    });
}

- (void)screenshotTaken:(NSNotification*)notification {
    dispatch_async(dispatch_get_main_queue(), ^{
        mm_restore_display(mm_scene, "screenshot");
    });
}

- (void)dealloc { [NSNotificationCenter.defaultCenter removeObserver:self]; }

- (void)sceneDidDisconnect:(UIScene*)scene {
    [NSNotificationCenter.defaultCenter removeObserver:self];
    if (scene == mm_scene) mm_scene = nil;
}
@end

namespace zelda64 {
// Runs the main run loop until the scene has connected (it does shortly
// after launch), so the first window can be shown.
void wait_for_ios_scene() {
    for (int i = 0; i < 300 && mm_scene == nil; ++i) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
}
}

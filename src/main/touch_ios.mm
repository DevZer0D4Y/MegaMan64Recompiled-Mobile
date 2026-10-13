#include "mobile_input.h"
#import <UIKit/UIKit.h>
#include <cmath>

@interface MMAnalogStick : UIView
@property(nonatomic, strong) UITouch* activeTouch;
@property(nonatomic) CGPoint offset;
@property(nonatomic) BOOL editing;
@property(nonatomic) NSInteger palette;
- (void)reset;
@end
@implementation MMAnalogStick
- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) { self.backgroundColor = UIColor.clearColor; self.accessibilityLabel = @"Movement stick"; }
    return self;
}
- (void)drawRect:(CGRect)rect {
    CGPoint center = CGPointMake(CGRectGetMidX(self.bounds), CGRectGetMidY(self.bounds));
    CGFloat radius = MIN(self.bounds.size.width, self.bounds.size.height) * .46;
    [[UIColor colorWithRed:.09 green:.20 blue:.30 alpha:.67] setFill];
    UIBezierPath* base = [UIBezierPath bezierPathWithOvalInRect:CGRectMake(center.x-radius,center.y-radius,radius*2,radius*2)];
    [base fill];
    UIColor* accent=self.palette==1?[UIColor colorWithRed:1 green:.77 blue:.36 alpha:.85]:(self.palette==2?[UIColor colorWithRed:.65 green:.84 blue:.58 alpha:.85]:[UIColor colorWithRed:.42 green:.85 blue:.94 alpha:.85]);
    [accent setStroke];base.lineWidth=2;[base stroke];
    CGFloat knob = radius * .38;
    [[accent colorWithAlphaComponent:.72] setFill];
    [[UIBezierPath bezierPathWithOvalInRect:CGRectMake(center.x+self.offset.x-knob,center.y+self.offset.y-knob,knob*2,knob*2)] fill];
}
- (void)updateTouch:(UITouch*)touch {
    CGPoint point = [touch locationInView:self];
    CGFloat radius = MIN(self.bounds.size.width,self.bounds.size.height)*.32;
    if (radius <= 0) return;
    CGFloat x=(point.x-CGRectGetMidX(self.bounds))/radius, y=(point.y-CGRectGetMidY(self.bounds))/radius;
    CGFloat length=hypot(x,y);
    if(length>1) { x/=length; y/=length; }
    self.offset=CGPointMake(x*radius,y*radius); [self setNeedsDisplay];
    CGFloat magnitude=MIN(length,1), output= magnitude <= .12 ? 0 : (magnitude-.12)/.88;
    if(magnitude>0) { x=x/magnitude*output; y=y/magnitude*output; }
    mobile::set_stick(x,-y);
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if(self.editing)return;
    if (!self.activeTouch) { self.activeTouch=touches.anyObject; [self updateTouch:self.activeTouch]; }
}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if(self.activeTouch && [touches containsObject:self.activeTouch]) [self updateTouch:self.activeTouch];
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    if(self.activeTouch && [touches containsObject:self.activeTouch]) [self reset];
}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event { [self reset]; }
- (void)reset { self.activeTouch=nil; self.offset=CGPointZero; mobile::set_stick(0,0); [self setNeedsDisplay]; }
@end

@interface MMPanelButton : UIButton
@property(nonatomic) NSInteger palette;
@end
@implementation MMPanelButton
- (void)drawRect:(CGRect)rect {
    CGFloat w=self.bounds.size.width,h=self.bounds.size.height,c=MIN(w,h)*.18;
    UIBezierPath* panel=[UIBezierPath bezierPath];
    [panel moveToPoint:CGPointMake(c,1)];[panel addLineToPoint:CGPointMake(w-c,1)];[panel addLineToPoint:CGPointMake(w-1,c)];
    [panel addLineToPoint:CGPointMake(w-1,h-c)];[panel addLineToPoint:CGPointMake(w-c,h-1)];[panel addLineToPoint:CGPointMake(c,h-1)];
    [panel addLineToPoint:CGPointMake(1,h-c)];[panel addLineToPoint:CGPointMake(1,c)];[panel closePath];
    CGContextRef context=UIGraphicsGetCurrentContext();CGContextSaveGState(context);[panel addClip];
    UIColor* top=self.highlighted?[UIColor colorWithRed:.22 green:.49 blue:.70 alpha:.93]:[UIColor colorWithRed:.14 green:.29 blue:.40 alpha:.81];
    if(!self.highlighted && self.palette==1)top=[UIColor colorWithRed:.09 green:.35 blue:.38 alpha:.81];
    if(!self.highlighted && self.palette==2)top=[UIColor colorWithRed:.27 green:.29 blue:.34 alpha:.81];
    NSArray* colors=@[(id)top.CGColor,(id)[UIColor colorWithRed:.035 green:.095 blue:.185 alpha:.72].CGColor];
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();CGGradientRef gradient=CGGradientCreateWithColors(space,(__bridge CFArrayRef)colors,nullptr);
    CGContextDrawLinearGradient(context,gradient,CGPointZero,CGPointMake(0,h),0);CGGradientRelease(gradient);CGColorSpaceRelease(space);CGContextRestoreGState(context);
    NSString* title=[self titleForState:UIControlStateNormal];
    UIColor* border=([title isEqual:@"A"]||[title isEqual:@"OK"])?[UIColor colorWithRed:1 green:.77 blue:.36 alpha:1]:[UIColor colorWithRed:.42 green:.85 blue:.94 alpha:1];
    if(self.palette==1)border=[UIColor colorWithRed:1 green:.77 blue:.36 alpha:1];
    if(self.palette==2)border=[UIColor colorWithRed:.65 green:.84 blue:.58 alpha:1];
    [border setStroke];panel.lineWidth=2;[panel stroke];
}
- (void)setHighlighted:(BOOL)value { [super setHighlighted:value];[self setNeedsDisplay]; }
@end

@interface MMTouchControl : UIView
@property(nonatomic) uint32_t pressed;
@property(nonatomic) int mode;
@property(nonatomic) BOOL navigationPad;
@property(nonatomic) BOOL editingControls;
@property(nonatomic) BOOL editingMenus;
@property(nonatomic) CGPoint dragOrigin;
@property(nonatomic) int heldNavigation;
@property(nonatomic, strong) NSTimer* repeatTimer;
@property(nonatomic, strong) MMAnalogStick* stick;
@property(nonatomic, strong) NSMutableArray<UIButton*>* buttons;
- (void)cancelInput;
@end
@implementation MMTouchControl
- (instancetype)initWithFrame:(CGRect)frame {
    self=[super initWithFrame:frame];
    if(self) {
        self.heldNavigation=-1;
        self.autoresizingMask=UIViewAutoresizingFlexibleWidth|UIViewAutoresizingFlexibleHeight;
        self.multipleTouchEnabled=YES;
        [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(cancelInput) name:UIApplicationWillResignActiveNotification object:nil];
        self.stick=[[MMAnalogStick alloc] initWithFrame:CGRectZero];self.stick.tag=24;[self addSubview:self.stick];
        UIPanGestureRecognizer* stickDrag=[[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(dragControl:)];stickDrag.enabled=NO;[self.stick addGestureRecognizer:stickDrag];
        self.buttons=[NSMutableArray array];
        NSArray* names=@[@"A",@"B",@"Z",@"L",@"R",@"START",@"C↑",@"C↓",@"C←",@"C→",@"D↑",@"D↓",@"D←",@"D→",@"MENU",@"PAD",@"↑",@"↓",@"←",@"→",@"OK",@"BACK",@"APPLY",@"EDIT"];
        for(NSUInteger i=0;i<names.count;i++) {
            UIButton* button=[MMPanelButton buttonWithType:UIButtonTypeCustom];button.tag=i;
            [button setTitle:names[i] forState:UIControlStateNormal];button.accessibilityLabel=names[i];
            [button setTitleColor:[UIColor colorWithRed:.89 green:.97 blue:1 alpha:1] forState:UIControlStateNormal];
            button.titleLabel.font=[UIFont systemFontOfSize:16 weight:UIFontWeightBold];button.titleLabel.adjustsFontSizeToFitWidth=YES;button.backgroundColor=UIColor.clearColor;
            [button addTarget:self action:@selector(down:) forControlEvents:UIControlEventTouchDown];
            [button addTarget:self action:@selector(up:) forControlEvents:UIControlEventTouchUpInside|UIControlEventTouchUpOutside|UIControlEventTouchCancel];
            if(i!=23){UIPanGestureRecognizer* drag=[[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(dragControl:)];drag.enabled=NO;[button addGestureRecognizer:drag];}
            [self.buttons addObject:button];[self addSubview:button];
        }
    }
    return self;
}
- (void)dealloc { [self.repeatTimer invalidate];[NSNotificationCenter.defaultCenter removeObserver:self]; }
- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
    UIView* hit=[super hitTest:point withEvent:event];return hit==self?nil:hit;
}
- (void)releaseNavigation {
    [self.repeatTimer invalidate];self.repeatTimer=nil;
    if(self.heldNavigation>=0) {
        const uint32_t masks[]={1<<18,1<<19,1<<17,1<<16,0x8000,0x4000,0x1000};
        mobile::menu_key(self.heldNavigation,false);self.pressed&=~masks[self.heldNavigation];mobile::set_touch(self.pressed);
    }
    self.heldNavigation=-1;
}
- (void)cancelInput {
    [self releaseNavigation];self.pressed=0;[self.stick reset];mobile::reset_touch();
    for(UIButton* button in self.buttons)button.highlighted=NO;
}
- (NSString*)layoutProfile {return (self.editingControls?self.editingMenus:(self.mode!=1||self.navigationPad))?@"touch.menu.":@"touch.game.";}
- (CGFloat)preference:(NSString*)name fallback:(CGFloat)value {
    id result=[NSUserDefaults.standardUserDefaults objectForKey:[[self layoutProfile] stringByAppendingString:name]];return result?[result doubleValue]:value;
}
- (void)dragControl:(UIPanGestureRecognizer*)gesture {
    if(!self.editingControls)return;
    UIView* view=gesture.view;CGRect area=UIEdgeInsetsInsetRect(self.bounds,self.safeAreaInsets);
    if(gesture.state==UIGestureRecognizerStateBegan)self.dragOrigin=view.center;
    CGPoint delta=[gesture translationInView:self];
    if(gesture.state==UIGestureRecognizerStateChanged) {
        CGFloat halfW=view.bounds.size.width/2,halfH=view.bounds.size.height/2;
        view.center=CGPointMake(MAX(area.origin.x+halfW,MIN(CGRectGetMaxX(area)-halfW,self.dragOrigin.x+delta.x)),MAX(area.origin.y+halfH,MIN(CGRectGetMaxY(area)-halfH,self.dragOrigin.y+delta.y)));
    }
    if((gesture.state==UIGestureRecognizerStateEnded||gesture.state==UIGestureRecognizerStateCancelled)&&area.size.width>0&&area.size.height>0) {
        NSString* key=[[self layoutProfile] stringByAppendingFormat:@"%ld",(long)view.tag];
        [NSUserDefaults.standardUserDefaults setObject:@{@"x":@((view.center.x-area.origin.x)/area.size.width),@"y":@((view.center.y-area.origin.y)/area.size.height)} forKey:key];
    }
}
- (void)editControls {
    [self cancelInput];UIViewController* host=self.window.rootViewController;
    if(host.presentedViewController)return;
    UIAlertController* dialog=[UIAlertController alertControllerWithTitle:self.editingControls?(self.editingMenus?@"Menu controls":@"Gameplay controls"):@"Edit touch controls" message:self.editingControls?@"Drag controls to move them.":@"Choose the layout to customize." preferredStyle:UIAlertControllerStyleAlert];
    if(!self.editingControls){
        for(int i=0;i<2;i++)[dialog addAction:[UIAlertAction actionWithTitle:i?@"Menu layout":@"Gameplay layout" style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){self.editingMenus=i==1;self.editingControls=YES;if(self.mode==1)mobile::open_menu();[self setNeedsLayout];}]];
        [dialog addAction:[UIAlertAction actionWithTitle:@"Cancel" style:UIAlertActionStyleCancel handler:nil]];
    }else{
        NSArray* labels=@[@"Save & done",@"Larger controls",@"Smaller controls",@"More visible",@"More transparent",@"Change color style",@"Reset this layout"];
        for(NSUInteger i=0;i<labels.count;i++)[dialog addAction:[UIAlertAction actionWithTitle:labels[i] style:UIAlertActionStyleDefault handler:^(UIAlertAction* action){
            NSUserDefaults* preferences=NSUserDefaults.standardUserDefaults;NSString* profile=[self layoutProfile];
            switch(i){
                case 0:self.editingControls=NO;break;
                case 1:[preferences setDouble:MIN(1.6,[self preference:@"scale" fallback:1]+.1) forKey:[profile stringByAppendingString:@"scale"]];break;
                case 2:[preferences setDouble:MAX(.7,[self preference:@"scale" fallback:1]-.1) forKey:[profile stringByAppendingString:@"scale"]];break;
                case 3:[preferences setDouble:MIN(1,[self preference:@"alpha" fallback:1]+.1) forKey:[profile stringByAppendingString:@"alpha"]];break;
                case 4:[preferences setDouble:MAX(.35,[self preference:@"alpha" fallback:1]-.1) forKey:[profile stringByAppendingString:@"alpha"]];break;
                case 5:[preferences setInteger:((int)[self preference:@"theme" fallback:0]+1)%3 forKey:[profile stringByAppendingString:@"theme"]];break;
                case 6:for(NSString* key in preferences.dictionaryRepresentation)if([key hasPrefix:profile])[preferences removeObjectForKey:key];break;
            }
            [self setNeedsLayout];
        }]];
        [dialog addAction:[UIAlertAction actionWithTitle:@"Keep editing" style:UIAlertActionStyleCancel handler:nil]];
    }
    [host presentViewController:dialog animated:YES completion:nil];
}
- (void)down:(UIButton*)button {
    NSInteger index=button.tag;
    if(index==23){[self editControls];return;}
    if(self.editingControls)return;
    if(index>=16) {
        [self releaseNavigation];int action=int(index-16);self.heldNavigation=action;
        if(self.mode==1) {
            const uint32_t masks[]={1<<18,1<<19,1<<17,1<<16,0x8000,0x4000,0x1000};self.pressed|=masks[action];mobile::set_touch(self.pressed);
        } else {
            mobile::menu_key(action,true);
            if(action<4) {
                self.repeatTimer=[NSTimer scheduledTimerWithTimeInterval:.095 repeats:YES block:^(NSTimer* timer){if(self.heldNavigation==action && self.mode!=1)mobile::menu_key(action,true);}];
                self.repeatTimer.fireDate=[NSDate dateWithTimeIntervalSinceNow:.38];
            }
        }
    } else if(index==15) {
        [self cancelInput];
        if(self.mode==3){self.navigationPad=NO;mobile::request_gamepad();}
        else self.navigationPad=!self.navigationPad;
        [self setNeedsLayout];
    }
    else if(index==14) { [self cancelInput];mobile::open_menu(); }
    else {
        const uint32_t masks[]={0x8000,0x4000,0x2000,0x20,0x10,0x1000,8,4,2,1,0x800,0x400,0x200,0x100};
        self.pressed|=masks[index];mobile::set_touch(self.pressed);
    }
}
- (void)up:(UIButton*)button {
    if(self.editingControls || button.tag==23)return;
    NSInteger index=button.tag;
    if(index>=16) {if(self.heldNavigation==index-16)[self releaseNavigation];}
    else if(index<14) {
        const uint32_t masks[]={0x8000,0x4000,0x2000,0x20,0x10,0x1000,8,4,2,1,0x800,0x400,0x200,0x100};
        self.pressed&=~masks[index];mobile::set_touch(self.pressed);
    }
}
- (void)layoutSubviews {
    [super layoutSubviews];CGRect area=UIEdgeInsetsInsetRect(self.bounds,self.safeAreaInsets);
    CGFloat w=area.size.width,h=area.size.height,scale=[self preference:@"scale" fallback:1],size=MIN(54,h*.15)*scale;
    BOOL menu=self.editingControls?self.editingMenus:(self.mode!=1||self.navigationPad);
    self.stick.editing=self.editingControls;
    const float positions[][2]={{.92,.68},{.83,.82},{.08,.12},{.22,.12},{.90,.12},{.44,.10},{.73,.39},{.73,.69},{.65,.54},{.81,.54},{.39,.55},{.39,.85},{.32,.70},{.46,.70},{.56,.10},{.68,.10},{.13,.65},{.13,.90},{.065,.775},{.195,.775},{.88,.85},{.75,.85},{.59,.85},{.32,.10}};
    for(NSUInteger i=0;i<self.buttons.count;i++) {
        UIButton* button=self.buttons[i];CGFloat width=(i==5||i==14||i==15||i>=20)?size*1.5:size;
        NSDictionary* saved=i==23?nil:[NSUserDefaults.standardUserDefaults objectForKey:[[self layoutProfile] stringByAppendingFormat:@"%lu",(unsigned long)i]];
        CGFloat x=saved?[saved[@"x"] doubleValue]:positions[i][0],y=saved?[saved[@"y"] doubleValue]:positions[i][1];
        button.frame=CGRectMake(area.origin.x+MAX(0,MIN(w-width,w*x-width/2)),area.origin.y+MAX(0,MIN(h-size,h*y-size/2)),width,size);
        BOOL visible=i==23||(i==15?(self.editingControls?!menu:(self.mode==1||self.mode==3)):(i>=16?menu:!menu));button.hidden=!visible;
        button.alpha=i==23?1:[self preference:@"alpha" fallback:1];((MMPanelButton*)button).palette=(NSInteger)[self preference:@"theme" fallback:0];
        for(UIGestureRecognizer* drag in button.gestureRecognizers)drag.enabled=self.editingControls;
        if(i==23)[button setTitle:self.editingControls?@"DONE":@"EDIT" forState:UIControlStateNormal];
        if(i==15){
            [button setTitle:menu?@"GAME PAD":@"MENU PAD" forState:UIControlStateNormal];
            button.accessibilityLabel=menu?@"Switch to gameplay pad":@"Switch to menu pad";
            [self bringSubviewToFront:button];
        }
        if(i==23)[self bringSubviewToFront:button];
        if(i==22)[button setTitle:self.mode==1?@"START":@"APPLY" forState:UIControlStateNormal];
        [button setNeedsDisplay];
    }
    CGFloat stickSize=MIN(155,h*.48)*scale;NSDictionary* savedStick=[NSUserDefaults.standardUserDefaults objectForKey:[[self layoutProfile] stringByAppendingString:@"24"]];
    CGFloat sx=savedStick?[savedStick[@"x"] doubleValue]:.14,sy=savedStick?[savedStick[@"y"] doubleValue]:.70;
    self.stick.frame=CGRectMake(area.origin.x+MAX(0,MIN(w-stickSize,w*sx-stickSize/2)),area.origin.y+MAX(0,MIN(h-stickSize,h*sy-stickSize/2)),stickSize,stickSize);
    self.stick.alpha=[self preference:@"alpha" fallback:1];self.stick.palette=(NSInteger)[self preference:@"theme" fallback:0];[self.stick setNeedsDisplay];for(UIGestureRecognizer* drag in self.stick.gestureRecognizers)drag.enabled=self.editingControls;
    self.stick.hidden=menu;self.hidden=self.mode==0;
}
@end
namespace mobile {
void set_controls_mode(int mode) {
    static int previous=-1;
    if(previous==mode) return;
    previous=mode; reset_touch();
    dispatch_async(dispatch_get_main_queue(), ^{
        static MMTouchControl* controls=nil;
        UIWindow* window=nil;
        for(UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
            if([scene isKindOfClass:UIWindowScene.class]) {
                for(UIWindow* candidate in ((UIWindowScene*)scene).windows) if(candidate.isKeyWindow) { window=candidate; break; }
            }
        }
        if(!window) window=UIApplication.sharedApplication.keyWindow;
        UIView* host=window.rootViewController.view;
        if(!host) { previous=-1; return; }
        if(!controls) controls=[[MMTouchControl alloc] initWithFrame:host.bounds];
        if(controls.superview!=host) { [controls removeFromSuperview]; controls.frame=host.bounds; [host addSubview:controls]; }
        [host bringSubviewToFront:controls]; [controls cancelInput]; controls.mode=mode; controls.navigationPad=NO; [controls setNeedsLayout];
    });
}
}

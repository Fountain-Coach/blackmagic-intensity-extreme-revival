// An explicitly opt-in, short-lived, local non-private HDMI source pattern.
// Show a deterministic moving pattern over the silver Mac's SINGLE final
// BMD HDMI 1920x1080@30 display so original Intensity IN can capture it.
// No network publishing, no alternative source, no mode changes, no BMD API.
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

@interface FountainHDMIPattern : NSView
@property(nonatomic,assign) NSUInteger frameIndex;
@end

@implementation FountainHDMIPattern
- (BOOL)isOpaque { return YES; }
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    const NSRect r=self.bounds;
    [[NSColor colorWithCalibratedWhite:0.12 alpha:1] setFill];
    NSRectFill(r);
    NSColor* colors[]={
        [NSColor whiteColor],[NSColor yellowColor],[NSColor cyanColor],
        [NSColor greenColor],[NSColor magentaColor],[NSColor redColor],
        [NSColor blueColor],[NSColor blackColor]
    };
    const CGFloat barWidth=r.size.width/8.0;
    for(int i=0;i<8;++i){
        [colors[i] setFill];
        NSRectFill(NSMakeRect(i*barWidth,r.size.height*0.12,
                            barWidth,r.size.height*0.44));
    }
    [[NSColor whiteColor] setFill];
    NSRectFill(NSMakeRect(0,0,r.size.width,r.size.height*0.08));
    const NSUInteger phase=self.frameIndex%80;
    [[NSColor blackColor] setFill];
    NSRectFill(NSMakeRect(phase*(r.size.width/80.0),0,
                          r.size.width/80.0,r.size.height*0.08));
    NSFont* font=[NSFont fontWithName:@"Menlo-Bold" size:72];
    if(!font)font=[NSFont boldSystemFontOfSize:72];
    NSDictionary* att=@{NSFontAttributeName:font,NSForegroundColorAttributeName:[NSColor whiteColor]};
    NSString* headline=[NSString stringWithFormat:@"SILVER HDMI / BLACKMAGIC INPUT"];
    [headline drawAtPoint:NSMakePoint(95,r.size.height*0.78) withAttributes:att];
    NSDictionary* small=@{NSFontAttributeName:[NSFont boldSystemFontOfSize:56],
                           NSForegroundColorAttributeName:[NSColor colorWithCalibratedWhite:0.9 alpha:1]};
    NSString* number=[NSString stringWithFormat:@"FRAME %06lu    1920x1080 / 30",
                     (unsigned long)self.frameIndex];
    [number drawAtPoint:NSMakePoint(95,r.size.height*0.62) withAttributes:small];
}
@end

@interface FountainTest : NSObject
@property(nonatomic,strong) FountainHDMIPattern* pattern;
- (void)tick:(NSTimer*)timer;
@end
@implementation FountainTest
- (void)tick:(NSTimer*)timer {
    (void)timer;
    self.pattern.frameIndex=self.pattern.frameIndex+1;
    [self.pattern setNeedsDisplay:YES];
}
@end

int main(int argc,char**argv) {
    if(argc!=2 || strcmp(argv[1],"--fixed-silver-pattern-20s")){
        fprintf(stderr,"opt-in --fixed-silver-pattern-20s only\n");
        return 64;
    }
    char model[64]={0};size_t cap=sizeof(model);
    if(sysctlbyname("hw.model",model,&cap,nullptr,0)||strcmp(model,"Macmini5,3"))
        return 65;
    @autoreleasepool {
        CGDirectDisplayID displays[32]={0};uint32_t count=0;
        if(CGGetOnlineDisplayList(32,displays,&count)!=kCGErrorSuccess)return 66;
        CGDirectDisplayID candidate=kCGNullDirectDisplay;int matches=0;
        for(uint32_t i=0;i<count;++i) {
            CGDirectDisplayID id=displays[i];
            if(CGDisplayVendorNumber(id)!=8961 ||
               CGDisplayPixelsWide(id)!=1920 || CGDisplayPixelsHigh(id)!=1080)
                continue;
            CGDisplayModeRef mode=CGDisplayCopyDisplayMode(id);
            double hz=mode?CGDisplayModeGetRefreshRate(mode):0;
            if(mode)CGDisplayModeRelease(mode);
            if(hz>29.9 && hz<30.1){candidate=id;++matches;}
        }
        if(matches!=1)return 67;
        NSApplication *app=[NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        NSScreen* screen=nil;
        for(NSScreen* s in [NSScreen screens]){
            NSNumber* n=s.deviceDescription[@"NSScreenNumber"];
            if(n && n.unsignedIntValue==candidate){screen=s;break;}
        }
        if(!screen)return 68;
        NSWindow* win=[[NSWindow alloc] initWithContentRect:screen.frame
                                    styleMask:NSWindowStyleMaskBorderless
                                      backing:NSBackingStoreBuffered
                                        defer:NO screen:screen];
        if(!win)return 69;
        FountainHDMIPattern* pattern=[[FountainHDMIPattern alloc] initWithFrame:
                                      NSMakeRect(0,0,screen.frame.size.width,screen.frame.size.height)];
        FountainTest* control=[FountainTest new];
        control.pattern=pattern;
        [win setContentView:pattern];
        [win setOpaque:YES];
        [win setLevel:NSStatusWindowLevel];
        [win setBackgroundColor:[NSColor blackColor]];
        [win orderFrontRegardless];
        NSTimer* timer=[NSTimer scheduledTimerWithTimeInterval:0.1
                         target:control selector:@selector(tick:) userInfo:nil repeats:YES];
        printf("stage=fixed_hdmi_pattern source=only_silver_hdmi"
               " mode=1920x1080@30 duration_s=20 private_desktop_hidden=1"
               " publishing=0\n");
        fflush(stdout);
        NSDate* until=[NSDate dateWithTimeIntervalSinceNow:20.0];
        while([until timeIntervalSinceNow]>0){
            [[NSRunLoop currentRunLoop] runUntilDate:
                           [NSDate dateWithTimeIntervalSinceNow:0.08]];
        }
        [timer invalidate];
        [win orderOut:nil];
        [win close];
        printf("stage=fixed_hdmi_pattern result=completed frames_drawn=%lu"
               " window_removed=1 source_mode_unchanged=1\n",
               (unsigned long)pattern.frameIndex);
        fflush(stdout);
    }
    return 0;
}

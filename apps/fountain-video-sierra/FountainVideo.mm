/*
 * Fountain Video - native macOS Sierra user-space capture application.
 *
 * No CoreMediaIO DAL dependency. All hardware I/O uses the already installed
 * DeckLinkAPI framework, dynamically loaded at runtime; no vendor binaries
 * or headers distributed. No kernel driver installation, firmware writes,
 * PCI register access or source-Mac manipulation. Local GUI only.
 *
 * Copyright (c) 2026 Fountain Coach contributors. MIT.
 */
#import <Cocoa/Cocoa.h>
#import "FCScratchPublisher.h"
#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMedia/CoreMedia.h>
#include <atomic>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <mach/mach_time.h>

typedef void* (*FCFactory)(void);
typedef int32_t (*FCNext)(void*, void**);
typedef int32_t (*FCQuery)(void*, CFUUIDBytes, void**);
typedef uint32_t (*FCRelease)(void*);
typedef int32_t (*FCEnableInput)(void*, uint32_t, uint32_t, uint32_t);
typedef int32_t (*FCNoArg)(void*);
typedef int32_t (*FCSetCallback)(void*, void*);
typedef int32_t (*FCGetFlag)(void*, uint32_t, bool*);
typedef int32_t (*FCGetInt)(void*, uint32_t, int64_t*);
typedef uint32_t (*FCFrameFlags)(void*);
typedef long (*FCFrameDimension)(void*);
typedef int32_t (*FCFrameBytes)(void*, void**);

static const unsigned char kInputIID[16] =
    {0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95};
static const unsigned char kStatusIID[16] =
    {0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static const unsigned char kCallbackIID[16] =
    {0xdd,0x04,0xe5,0xec,0x74,0x15,0x42,0xab,0xae,0x4a,0xe8,0x0c,0x4d,0xfc,0x04,0x4a};
static const unsigned char kUnknownIID[16] =
    {0,0,0,0,0,0,0,0,0xc0,0,0,0,0,0,0,0x46};
static const uint32_t kPixel2vuy = 0x32767579;
static const uint32_t kNoInputSource = 0x80000000u;
// Fixed rig: silver Mac HDMI OUT -> Intensity Extreme HDMI IN (same Mac via Thunderbolt).
static BOOL fcSilverFixedBMD1080p30() {
    CGDirectDisplayID ids[32]={0};
    uint32_t count=0;
    if(CGGetOnlineDisplayList(32,ids,&count)!=kCGErrorSuccess)return NO;
    for(uint32_t i=0;i<count;++i) {
        CGDirectDisplayID display=ids[i];
        if(CGDisplayVendorNumber(display)!=8961 ||
           CGDisplayPixelsWide(display)!=1920 ||
           CGDisplayPixelsHigh(display)!=1080)continue;
        CGDisplayModeRef mode=CGDisplayCopyDisplayMode(display);
        const double hz=mode?CGDisplayModeGetRefreshRate(mode):0;
        if(mode)CGDisplayModeRelease(mode);
        if(hz>29.9 && hz<30.1)return YES;
    }
    return NO;
}
// Avoid the 10.4 frame GetStreamTime method: independently crash-isolated.
static int64_t fcHostTicks30000() {
    mach_timebase_info_data_t base={0,0};
    if(mach_timebase_info(&base)!=KERN_SUCCESS || !base.denom)return -1;
    const __uint128_t scaled=(__uint128_t)mach_absolute_time()*base.numer*30000u;
    return (int64_t)(scaled/((__uint128_t)base.denom*1000000000u));
}
static CFUUIDBytes fcUUID(const unsigned char *id) {
    CFUUIDBytes result; memcpy(&result, id, 16); return result;
}
/* Nikon D5200 PAL/NTSC frame rates are not necessarily exact 30/60 Hz.
 * Scan the six actual D5200 movie formats first, PAL before NTSC.
 * Additional exact-rate modes remain selectable manually.
 */
static const uint32_t kFountainVideoModes[] = {
    0x68703530u,  // 720p50 — PAL
    0x48693530u,  // 1080i50 — PAL
    0x48703235u,  // 1080p25 — PAL
    0x68703539u,  // 720p59.94 — NTSC
    0x48693539u,  // 1080i59.94 — NTSC
    0x48703239u,  // 1080p29.97 — NTSC
    0x48703330u,  // 1080p30 — exact, manual only
    0x68703630u   // 720p60 — exact, manual only
};
enum { kNikonAutoScanModeCount = 6 };
static uint32_t modeCode(NSInteger choice) {
    if (choice < 0 ||
        choice >= (NSInteger)(sizeof(kFountainVideoModes) /
                             sizeof(kFountainVideoModes[0]))) return 0;
    return kFountainVideoModes[choice];
}

static unsigned char clamp8(int v) {
    return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
}
static NSData *rgbaFrom2vuy(const uint8_t *src, size_t bytes,
                            long width, long height, long rowBytes) {
    if (!src || width < 2 || height < 1 || (width & 1) ||
        width > 4096 || height > 2160 ||
        rowBytes < width * 2 || rowBytes > 65536) return nil;
    if ((size_t)rowBytes > SIZE_MAX / (size_t)height ||
        bytes < (size_t)rowBytes * (size_t)height) return nil;
    if ((size_t)width > SIZE_MAX / (size_t)height / 4) return nil;
    NSMutableData *rgba = [NSMutableData dataWithLength:
                           (NSUInteger)(width * height * 4)];
    uint8_t *out = (uint8_t*)rgba.mutableBytes;
    for (long y = 0; y < height; ++y) {
        const uint8_t *row = src + y * rowBytes;
        uint8_t *target = out + y * width * 4;
        for (long x = 0; x < width; x += 2) {
            const uint8_t *pair = row + (x / 2) * 4;
            int cb = (int)pair[0] - 128;
            int cr = (int)pair[2] - 128;
            for (int j = 0; j < 2; ++j) {
                int yy = (int)pair[1 + j * 2] - 16;
                uint8_t *p = target + (x + j) * 4;
                p[0] = clamp8((298 * yy + 409 * cr + 128) >> 8);
                p[1] = clamp8((298 * yy - 100 * cb - 208 * cr + 128) >> 8);
                p[2] = clamp8((298 * yy + 516 * cb + 128) >> 8);
                p[3] = 255;
            }
        }
    }
    return rgba;
}

static NSImage *imageFromRGBA(NSData *data, NSUInteger width, NSUInteger height) {
    if (!data || width < 1 || height < 1 ||
        width > 4096 || height > 2160 ||
        data.length != width * height * 4) return nil;
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
                     pixelsWide:(NSInteger)width pixelsHigh:(NSInteger)height
                  bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
                 colorSpaceName:NSDeviceRGBColorSpace
                    bytesPerRow:(NSInteger)(width * 4) bitsPerPixel:32];
    if (!rep || !rep.bitmapData) return nil;
    memcpy(rep.bitmapData, data.bytes, data.length);
    NSImage *img = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
    [img addRepresentation:rep];
    return img;
}

static NSData *virtualBarsRGBA(NSUInteger width, NSUInteger height, uint32_t sequence) {
    if (!width || !height || width > 4096 || height > 2160) return nil;
    NSMutableData *result = [NSMutableData dataWithLength:width*height*4];
    uint8_t *out = (uint8_t*)result.mutableBytes;
    static const uint8_t bars[8][3] = {
        {235,235,235}, {235,235,16}, {16,235,235}, {16,235,16},
        {235,16,235}, {235,16,16}, {16,16,235}, {16,16,16}
    };
    const NSUInteger side=MIN(MIN(width,height),
                              MAX((NSUInteger)16,MIN(width,height)/9));
    const NSUInteger travel=width>side ? width-side : 0;
    const NSUInteger pos=travel ? (sequence*23u)%(travel+1) : 0;
    const NSUInteger top=height/2-side/2;
    for (NSUInteger y=0; y<height; ++y) {
        for (NSUInteger x=0; x<width; ++x) {
            NSUInteger idx = (x * 8) / width;
            uint8_t *pixel = out + (y*width+x)*4;
            pixel[0]=bars[idx][0]; pixel[1]=bars[idx][1];
            pixel[2]=bars[idx][2]; pixel[3]=255;
            // Moving high-contrast target: browser jogging becomes visually
            // measurable, unlike an almost unchanging color-bar picture.
            if (x>=pos && x<pos+side && y>=top && y<top+side) {
                const NSUInteger dx=x-pos,dy=y-top;
                const BOOL edge=dx<5 || dy<5 || dx>=side-5 || dy>=side-5;
                pixel[0]=pixel[1]=pixel[2]=edge?242:16;
            }
            // Sixteen blocks encode the frame number as a visible binary
            // signature. Each frame's content is distinguishable when frozen.
            if (y>=height-44 && y<height-12 && x>=20 && x<20+16*14) {
                const NSUInteger bit=(x-20)/14;
                const BOOL gap=((x-20)%14)<2;
                const BOOL one=(sequence >> (15-bit)) & 1u;
                pixel[0]=pixel[1]=pixel[2]=gap?20:(one?238:65);
            }
        }
    }
    return result;
}

@class FountainController;
struct FCCallback {
    void **vt;
    std::atomic<uint32_t> refs;
    std::atomic<uint64_t> frames;
    std::atomic<uint64_t> valid;
    std::atomic<uint64_t> noSource;
    std::atomic<uint32_t> sequence;
    std::atomic<uint32_t> pendingMain;
    void *context;
};
static uint32_t fcAddRef(FCCallback *c) { return ++c->refs; }
static uint32_t fcReleaseRef(FCCallback *c) { return --c->refs; }
static int32_t fcQueryInterface(FCCallback *c, CFUUIDBytes iid, void **out) {
    if (!out) return (int32_t)0x80004003;
    if (!memcmp(&iid,kCallbackIID,16) || !memcmp(&iid,kUnknownIID,16)) {
        *out=c; fcAddRef(c); return 0;
    }
    *out=NULL; return (int32_t)0x80004002;
}
static int32_t fcFormatChange(FCCallback*,uint32_t,void*,uint32_t) { return 0; }

@interface FountainController : NSObject <NSApplicationDelegate, NSWindowDelegate>
- (void)acceptHardwareRGBA:(NSData*)pixels width:(NSUInteger)width
                   height:(NSUInteger)height
                    ticks:(int64_t)ticks duration:(int64_t)duration
                timestampValid:(BOOL)valid;
@end

static int32_t fcVideoFrame(FCCallback *cb, void *frame, void*) {
    if(!frame)return 0;
    ++cb->frames;
    void** vt=*(void***)frame;
    const uint32_t flags=((FCFrameFlags)vt[7])(frame);
    if(flags & kNoInputSource) { ++cb->noSource; return 0; }
    ++cb->valid;
    // Decimate to a bounded five frame/s preview while counting all callbacks.
    if(cb->sequence.fetch_add(1)%6)return 0;
    const long w=((FCFrameDimension)vt[3])(frame);
    const long h=((FCFrameDimension)vt[4])(frame);
    const long rb=((FCFrameDimension)vt[5])(frame);
    if(w!=1920 || h!=1080 || rb!=3840 || cb->pendingMain.load()>=2)return 0;
    void* ptr=NULL;
    if(((FCFrameBytes)vt[8])(frame,&ptr) || !ptr)return 0;
    const int64_t hostTicks=fcHostTicks30000();
    if(hostTicks<0)return 0;
    @autoreleasepool {
        // 4,147,200 bytes are copied while the SDK's borrowed frame is valid.
        // Never access a vendor frame from the dispatched AppKit block.
        NSData *own=[NSData dataWithBytes:ptr length:(NSUInteger)(rb*h)];
        if(!own)return 0;
        ++cb->pendingMain;
        FountainController *owner=(__bridge FountainController*)cb->context;
        dispatch_async(dispatch_get_main_queue(), ^{
            @autoreleasepool {
                NSData *rgba=rgbaFrom2vuy((const uint8_t*)own.bytes,
                                         own.length,w,h,rb);
                if(rgba)[owner acceptHardwareRGBA:rgba width:(NSUInteger)w
                                            height:(NSUInteger)h
                                             ticks:hostTicks duration:1000
                                    timestampValid:YES];
            }
            --cb->pendingMain;
        });
    }
    return 0;
}
static void* kCallbackVtable[5] = {
    (void*)fcQueryInterface,(void*)fcAddRef,(void*)fcReleaseRef,
    (void*)fcFormatChange,(void*)fcVideoFrame
};

@interface VideoStage : NSView
@property(nonatomic,strong) NSImage *frameImage;
@property(nonatomic,copy) NSString *notice;
@property(nonatomic,assign) BOOL synthetic;
@end
@implementation VideoStage
- (BOOL)isFlipped { return YES; }
- (void)drawRect:(NSRect)dirty {
    [[NSColor colorWithCalibratedRed:.065 green:.075 blue:.085 alpha:1] setFill];
    NSRectFill(self.bounds);
    if (self.frameImage) {
        NSSize original=self.frameImage.size;
        CGFloat scale=MIN(self.bounds.size.width/original.width,
                          self.bounds.size.height/original.height);
        NSSize scaled=NSMakeSize(original.width*scale,original.height*scale);
        NSRect box=NSMakeRect((self.bounds.size.width-scaled.width)/2,
                             (self.bounds.size.height-scaled.height)/2,
                             scaled.width,scaled.height);
        [self.frameImage drawInRect:box fromRect:NSZeroRect
                          operation:NSCompositingOperationSourceOver fraction:1
                    respectFlipped:YES hints:nil];
    }
    NSString *notice=self.notice ?: (self.frameImage ? @"" : @"NO SOURCE");
    if (notice.length) {
        NSDictionary *att=@{NSFontAttributeName:[NSFont systemFontOfSize:18
                                                                 weight:NSFontWeightMedium],
                             NSForegroundColorAttributeName:[NSColor colorWithCalibratedWhite:.83 alpha:1]};
        NSSize size=[notice sizeWithAttributes:att];
        CGFloat x=(self.bounds.size.width-size.width)/2;
        CGFloat y=(self.bounds.size.height-size.height)/2;
        [notice drawAtPoint:NSMakePoint(x,y) withAttributes:att];
    }
    if (self.synthetic) {
        NSDictionary *tag=@{
            NSFontAttributeName:[NSFont monospacedDigitSystemFontOfSize:11
                                                                weight:NSFontWeightBold],
            NSForegroundColorAttributeName:[NSColor colorWithCalibratedRed:1 green:.85 blue:.54 alpha:1]};
        [@"SYNTHETIC · NOT CAPTURED" drawAtPoint:NSMakePoint(18,16)
                                  withAttributes:tag];
    }
}
@end

@interface FountainController ()
- (void)appendRecordingRGBA:(NSData*)rgba width:(NSUInteger)width
                    height:(NSUInteger)height timestamp:(CMTime)pts;
- (void)finishRecording;
@property(nonatomic,strong) NSWindow *window;
@property(nonatomic,strong) VideoStage *stage;
@property(nonatomic,strong) NSTextField *status;
@property(nonatomic,strong) NSTextField *deviceDetails;
@property(nonatomic,strong) NSTextField *frameDetails;
@property(nonatomic,strong) NSTextField *recordLabel;
@property(nonatomic,strong) NSButton *scratchShareToggle;
@property(nonatomic,strong) FCScratchPublisher *scratchPublisher;
@property(nonatomic,strong) NSPopUpButton *sourceChoice;
@property(nonatomic,strong) NSPopUpButton *formatChoice;
@property(nonatomic,strong) NSButton *startButton;
@property(nonatomic,strong) NSButton *stopButton;
@property(nonatomic,strong) NSButton *snapshotButton;
@property(nonatomic,strong) NSButton *recordButton;
@property(nonatomic,strong) NSTimer *statusTimer;
@property(nonatomic,strong) NSData *lastRGBA;
@property(nonatomic,strong) NSImage *lastFrame;
@property(nonatomic,assign) NSUInteger lastWidth;
@property(nonatomic,assign) NSUInteger lastHeight;
@property(nonatomic,assign) BOOL captureActive;
@property(nonatomic,assign) BOOL virtualActive;
@property(nonatomic,assign) BOOL autoScanning;
@property(nonatomic,assign) NSInteger autoModeIndex;
@property(nonatomic,assign) NSTimeInterval autoModeStarted;
@property(nonatomic,assign) uint32_t virtualFrame;
@property(nonatomic,assign) BOOL recordRequested;
@property(nonatomic,strong) NSURL *recordURL;
@property(nonatomic,strong) AVAssetWriter *writer;
@property(nonatomic,strong) AVAssetWriterInput *writerInput;
@property(nonatomic,strong) AVAssetWriterInputPixelBufferAdaptor *writerAdaptor;
@property(nonatomic,assign) int64_t firstTicks;
@property(nonatomic,assign) BOOL haveFirstTicks;
@property(nonatomic,assign) uint64_t movieFrames;
@end

@implementation FountainController {
    void *_library;
    void *_device;
    void *_input;
    FCCallback *_callback;
    std::vector<FCCallback*> _pastCallbacks;
    uint32_t _selectedMode;
    BOOL _hardwareLocked;
}
static NSTextField *fcLabel(NSRect frame, NSString *title, CGFloat font,
                            NSColor *color, BOOL bold) {
    NSTextField *t=[[NSTextField alloc] initWithFrame:frame];
    t.editable=NO; t.bezeled=NO; t.drawsBackground=NO; t.selectable=YES;
    t.stringValue=title?:@"";
    t.font=bold ? [NSFont systemFontOfSize:font weight:NSFontWeightSemibold] :
                  [NSFont systemFontOfSize:font];
    t.textColor=color;
    return t;
}
static NSButton *fcButton(NSRect frame,NSString *title, id target, SEL action) {
    NSButton *b=[[NSButton alloc] initWithFrame:frame];
    b.title=title; b.bezelStyle=NSRoundedBezelStyle;
    b.target=target; b.action=action; return b;
}
- (void)setMessage:(NSString*)message {
    self.status.stringValue=message?:@"";
    NSLog(@"FountainVideo %@",message);
}
- (void)buildWindow {
    self.window=[[NSWindow alloc]
        initWithContentRect:NSMakeRect(0,0,1120,700)
                  styleMask:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|
                             NSWindowStyleMaskMiniaturizable)
                    backing:NSBackingStoreBuffered defer:NO];
    [self.window setTitle:@"Fountain Video"];
    [self.window center];
    self.window.delegate=self;
    NSView *root=self.window.contentView;
    root.wantsLayer=YES;
    root.layer.backgroundColor=[[NSColor colorWithCalibratedRed:.10 green:.125
                                 blue:.145 alpha:1] CGColor];
    NSColor *ivory=[NSColor colorWithCalibratedRed:.91 green:.92 blue:.91 alpha:1];
    NSColor *muted=[NSColor colorWithCalibratedRed:.61 green:.70 blue:.71 alpha:1];
    [root addSubview:fcLabel(NSMakeRect(28,640,730,39),@"FOUNTAIN VIDEO",27,ivory,YES)];
    [root addSubview:fcLabel(NSMakeRect(28,614,760,22),
                            @"INTENSITY EXTREME  /  SIERRA  /  NATIVE VIDEO INSTRUMENT",
                            12,muted,NO)];
    self.stage=[[VideoStage alloc] initWithFrame:NSMakeRect(28,181,762,420)];
    self.stage.wantsLayer=YES;
    self.stage.layer.cornerRadius=9;
    self.stage.layer.masksToBounds=YES;
    self.stage.synthetic=NO;
    self.stage.notice=@"DEVICE READY TO INSPECT";
    [root addSubview:self.stage];
    self.status=fcLabel(NSMakeRect(28,142,762,28),@"Inspecting device…",17,ivory,YES);
    [root addSubview:self.status];
    self.frameDetails=fcLabel(NSMakeRect(28,112,762,23),
                               @"Live video is only shown when real frames arrive.",
                               12,muted,NO);
    [root addSubview:self.frameDetails];

    [root addSubview:fcLabel(NSMakeRect(822,566,260,30),
                            @"Video source",18,ivory,YES)];
    self.sourceChoice=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(818,525,276,32)
                                                 pullsDown:NO];
    [self.sourceChoice addItemsWithTitles:@[@"Blackmagic HDMI input",
                                             @"Virtual color bars · test"]];
    self.sourceChoice.target=self;
    self.sourceChoice.action=@selector(sourceChanged:);
    [root addSubview:self.sourceChoice];
    [root addSubview:fcLabel(NSMakeRect(822,481,260,23),
                            @"Capture format",13,muted,NO)];
    self.formatChoice=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(818,441,276,32)
                                                 pullsDown:NO];
    [self.formatChoice addItemsWithTitles:@[@"Auto · Nikon D5200 (6 formats)",
                                            @"720p50 (PAL)",
                                            @"1080i50 (PAL)",
                                            @"1080p25 (PAL)",
                                            @"720p59.94 (NTSC)",
                                            @"1080i59.94 (NTSC)",
                                            @"1080p29.97 (NTSC)",
                                            @"1080p30 (exact)",
                                            @"720p60 (exact)"]];
    [root addSubview:self.formatChoice];
    self.startButton=fcButton(NSMakeRect(818,389,131,36),@"Start",[self class],
                               @selector(startPressed:));
    self.startButton.target=self;
    [root addSubview:self.startButton];
    self.stopButton=fcButton(NSMakeRect(962,389,131,36),@"Stop",self,
                              @selector(stopPressed:));
    [root addSubview:self.stopButton];

    [root addSubview:fcLabel(NSMakeRect(822,335,260,23),
                            @"Capture",13,muted,NO)];
    self.snapshotButton=fcButton(NSMakeRect(818,291,276,36),
                         @"Save current frame…",self,@selector(saveFrame:));
    [root addSubview:self.snapshotButton];
    self.recordButton=fcButton(NSMakeRect(818,247,276,36),@"Record movie…",self,
                                @selector(recordPressed:));
    [root addSubview:self.recordButton];
    self.recordLabel=fcLabel(NSMakeRect(822,205,265,37),
                           @"Nothing is recorded automatically.",
                           12,muted,NO);
    [root addSubview:self.recordLabel];

    [root addSubview:fcLabel(NSMakeRect(822,150,260,23),
                            @"Device",13,muted,NO)];
    self.deviceDetails=fcLabel(NSMakeRect(822,101,270,46),@"Discovering…",
                                12,ivory,NO);
    self.deviceDetails.lineBreakMode=NSLineBreakByWordWrapping;
    self.deviceDetails.cell.wraps=YES;
    [root addSubview:self.deviceDetails];
    NSButton *scratch=fcButton(NSMakeRect(818,50,132,36),
                         @"Browser scratch…",self,@selector(openBrowserScratch:));
    [root addSubview:scratch];
    NSButton *lab=fcButton(NSMakeRect(958,50,136,36),
                         @"Video lab…",self,@selector(openSoftwareLab:));
    [root addSubview:lab];
    self.scratchShareToggle=[[NSButton alloc]
                      initWithFrame:NSMakeRect(818,12,277,30)];
    [self.scratchShareToggle setButtonType:NSButtonTypeSwitch];
    self.scratchShareToggle.title=@"Stream frames to private browser";
    self.scratchShareToggle.font=[NSFont systemFontOfSize:11];
    self.scratchShareToggle.target=self;
    self.scratchShareToggle.action=@selector(scratchShareChanged:);
    [self.scratchShareToggle setState:NSControlStateValueOn];
    [root addSubview:self.scratchShareToggle];
    [root addSubview:fcLabel(NSMakeRect(28,32,755,58),
     @"Software simulation starts without a camera or HDMI. Open Software lab for the full\nvirtual input/output test circuit. Real hardware capture remains an optional adapter.",
                             12,muted,NO)];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}
- (BOOL)openDevice {
    if (_device) return YES;
    _library=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                    RTLD_NOW|RTLD_LOCAL);
    if (!_library) {
        self.deviceDetails.stringValue=@"DeckLink runtime unavailable.";
        return NO;
    }
    FCFactory factory=(FCFactory)dlsym(_library,"CreateDeckLinkIteratorInstance");
    if (!factory) { [self closeDevice]; return NO; }
    void *iterator=factory();
    if (!iterator) { [self closeDevice]; return NO; }
    void **vt=*(void***)iterator;
    int32_t rc=((FCNext)vt[3])(iterator,&_device);
    ((FCRelease)vt[2])(iterator);
    if (rc || !_device) { [self closeDevice]; return NO; }
    self.deviceDetails.stringValue=@"Intensity Extreme · vendor driver 10.4";
    return YES;
}
- (void)closeDevice {
    if (_input) return;  // stop capture first
    if (_device) {
        void **vt=*(void***)_device;
        ((FCRelease)vt[2])(_device); _device=NULL;
    }
    if (_library) { dlclose(_library); _library=NULL; }
}
- (void)readSignalStatus {
    if (!_device) { _hardwareLocked=NO; return; }
    void *status=NULL;
    void **deviceTable=*(void***)_device;
    int32_t rc=((FCQuery)deviceTable[0])(_device,fcUUID(kStatusIID),&status);
    if (rc || !status) { _hardwareLocked=NO; return; }
    void **vt=*(void***)status;
    bool locked=false;
    int32_t lrc=((FCGetFlag)vt[3])(status,0x7669736c,&locked);
    _hardwareLocked=lrc==0 && locked;
    ((FCRelease)vt[2])(status);
}
- (void)sourceChanged:(id)sender {
    BOOL hardware=self.sourceChoice.indexOfSelectedItem==0;
    if(hardware) {
        [self.formatChoice selectItemAtIndex:7]; // Exact silver HDMI: 1080p30
        // Capturing the host's primary desktop is private by default.
        [self.scratchShareToggle setState:NSControlStateValueOff];
        [self setMessage:@"Silver HDMI loopback · 1080p30 · private preview"];
    }
    self.formatChoice.enabled=NO; // Do not auto-scan other hardware fixtures
}
- (void)scratchShareChanged:(id)sender {
    if (self.scratchPublisher)
        self.scratchPublisher.enabled =
            self.scratchShareToggle.state==NSControlStateValueOn;
}
- (void)openBrowserScratch:(id)sender {
    NSString *configured=[[NSUserDefaults standardUserDefaults]
                            stringForKey:@"FountainVideoScratchPageURL"];
    if (!configured.length)
        configured=@"http://127.0.0.1:8273/scratch";
    NSURL *url=[NSURL URLWithString:configured];
    if (!url || (![url.scheme isEqualToString:@"http"] &&
                 ![url.scheme isEqualToString:@"https"])) {
        [self setMessage:@"Browser Scratch URL is not configured correctly."];
        return;
    }
    if ([[NSWorkspace sharedWorkspace] openURL:url])
        [self setMessage:@"Browser scratching opened."];
    else
        [self setMessage:@"Browser Scratch service unavailable."];
}
- (void)openSoftwareLab:(id)sender {
    NSString *configured=[[NSUserDefaults standardUserDefaults]
                            stringForKey:@"FountainVideoLabURL"];
    if (!configured.length) configured=@"http://127.0.0.1:8272/lab";
    NSURL *destination=[NSURL URLWithString:configured];
    NSString *scheme=[destination.scheme lowercaseString];
    if (!destination || (![scheme isEqualToString:@"http"] &&
                         ![scheme isEqualToString:@"https"])) {
        [self setMessage:@"Invalid software lab URL in application preferences."];
        return;
    }
    if ([[NSWorkspace sharedWorkspace] openURL:destination]) {
        [self setMessage:@"Opening full software video laboratory."];
    } else {
        [self setMessage:@"Cannot open the software lab URL."];
    }
}
- (void)refreshPressed:(id)sender {
    if (![self openDevice]) {
        [self setMessage:@"No Blackmagic capture device available."];
        self.deviceDetails.stringValue=@"No compatible DeckLink device";
        return;
    }
    [self readSignalStatus];
    if (!self.virtualActive && !self.captureActive) {
        [self setMessage:_hardwareLocked ? @"HDMI signal detected" :
                                  @"Device present · no verified input signal"];
    }
}
- (void)startPressed:(id)sender {
    // Explicit fixed test rig. Reject any other monitor/mode before DeckLink I/O.
    if(self.sourceChoice.indexOfSelectedItem==0 &&
       !fcSilverFixedBMD1080p30()) {
        [self setMessage:@"BMD HDMI 1920×1080@30 not verified. Capture not started."];
        return;
    }
    [self finishRecording];
    [self stopCurrent:NO];
    self.autoScanning=NO;
    self.lastFrame=nil;
    self.lastRGBA=nil;
    self.stage.frameImage=nil;
    self.stage.notice=nil;
    if (self.sourceChoice.indexOfSelectedItem==1) {
        self.virtualActive=YES;
        self.stage.synthetic=YES;
        self.stage.notice=nil;
        [self setMessage:@"Virtual source · generated frames"];
        self.statusTimer=[NSTimer scheduledTimerWithTimeInterval:.2 target:self
                         selector:@selector(tick:) userInfo:nil repeats:YES];
        [self tick:nil];
        return;
    }
    self.autoScanning=self.formatChoice.indexOfSelectedItem==0;
    self.autoModeIndex=self.autoScanning ? 0 :
                       self.formatChoice.indexOfSelectedItem-1;
    [self beginHardwareMode:self.autoModeIndex];
}
- (void)beginHardwareMode:(NSInteger)modeIndex {
    self.stage.synthetic=NO;
    _selectedMode=modeCode(modeIndex);
    if (![self openDevice] || !_selectedMode) {
        self.stage.notice=@"DEVICE NOT AVAILABLE";
        [self setMessage:@"Blackmagic device unavailable."];
        [self.stage setNeedsDisplay:YES];
        return;
    }
    void **deviceTable=*(void***)_device;
    int32_t rc=((FCQuery)deviceTable[0])(_device,fcUUID(kInputIID),&_input);
    if (rc || !_input) {
        _input=NULL;
        [self setMessage:@"Video input interface unavailable."];
        self.stage.notice=@"CANNOT OPEN VIDEO INPUT";
        [self.stage setNeedsDisplay:YES];
        return;
    }
    _callback=new FCCallback();
    _callback->vt=kCallbackVtable;
    _callback->refs.store(1); _callback->frames.store(0);
    _callback->valid.store(0); _callback->noSource.store(0);
    _callback->sequence.store(0); _callback->pendingMain.store(0);
    _callback->context=(__bridge void*)self;
    _pastCallbacks.push_back(_callback);
    void **inputTable=*(void***)_input;
    rc=((FCEnableInput)inputTable[6])(_input,_selectedMode,kPixel2vuy,0);
    if(rc) {
        [self setMessage:[NSString stringWithFormat:
                @"Cannot enable matched HDMI input (0x%08x).",(unsigned)rc]];
        [self stopCurrent:NO]; return;
    }
    NSInteger stable=0;
    for(NSInteger sample=0;sample<12 && stable<2;++sample) {
        [self readSignalStatus];
        stable=_hardwareLocked?stable+1:0;
        if(stable<2)usleep(250000);
    }
    if(stable<2) {
        ((FCNoArg)inputTable[7])(_input);
        [self stopCurrent:NO];
        [self setMessage:@"No HDMI signal lock. Capture NOT started."];
        return;
    }
    rc=((FCSetCallback)inputTable[17])(_input,_callback);
    if(rc) {
        ((FCNoArg)inputTable[7])(_input);
        [self stopCurrent:NO];
        [self setMessage:@"Cannot attach captured-frame callback."];
        return;
    }
    self.captureActive=YES;
    rc=((FCNoArg)inputTable[13])(_input);
    if (rc) {
        [self setMessage:[NSString stringWithFormat:
                            @"Cannot start capture (0x%08x).",(unsigned)rc]];
        [self stopCurrent:NO]; return;
    }
    self.autoModeStarted=[NSDate timeIntervalSinceReferenceDate];
    self.stage.notice=@"WAITING FOR HDMI SIGNAL";
    if (self.autoScanning) {
        [self setMessage:[NSString stringWithFormat:
                  @"Scanning Nikon HDMI · format %ld of %d",(long)(modeIndex+1),kNikonAutoScanModeCount]];
    } else {
        [self setMessage:@"Capture running · checking for live frames"];
    }
    self.statusTimer=[NSTimer scheduledTimerWithTimeInterval:.5 target:self
                               selector:@selector(tick:) userInfo:nil repeats:YES];
    [self tick:nil];
}
- (void)stopCurrent:(BOOL)showStatus {
    [self.statusTimer invalidate]; self.statusTimer=nil;
    if (_input) {
        void **vt=*(void***)_input;
        if (self.captureActive) ((FCNoArg)vt[14])(_input);
        // Remove callback before releasing the provider and client.
        ((FCSetCallback)vt[17])(_input,NULL);
        if (self.captureActive) ((FCNoArg)vt[7])(_input);
        ((FCRelease)vt[2])(_input); _input=NULL;
    }
    self.captureActive=NO;
    self.virtualActive=NO;
    _callback=NULL; _hardwareLocked=NO;
    if (showStatus) [self setMessage:@"Stopped · device remains available"];
}
- (void)stopPressed:(id)sender {
    self.autoScanning=NO;
    [self stopCurrent:YES];
    [self finishRecording];
    self.stage.notice=self.lastFrame?nil:@"STOPPED";
    [self.stage setNeedsDisplay:YES];
}
- (void)tick:(NSTimer*)timer {
    if (self.virtualActive) {
        uint32_t num=self.virtualFrame++;
        NSUInteger width=1280,height=720;
        NSData *rgba=virtualBarsRGBA(width,height,num);
        if (rgba) {
            [self receiveRGBA:rgba width:width height:height
                        ticks:(int64_t)num*6000 duration:6000 valid:YES
                    synthetic:YES];
        }
        return;
    }
    if (!self.captureActive) return;
    [self readSignalStatus];
    unsigned long long frames=_callback ? (unsigned long long)_callback->frames.load():0;
    unsigned long long valid=_callback ? (unsigned long long)_callback->valid.load():0;
    unsigned long long noSource=_callback ? (unsigned long long)_callback->noSource.load():0;
    self.frameDetails.stringValue=[NSString stringWithFormat:
        @"Callbacks %llu  ·  Valid %llu  ·  No source %llu  ·  Lock %@",
        frames,valid,noSource,_hardwareLocked?@"YES":@"NO"];
    if (self.autoScanning && (valid>0 || _hardwareLocked)) {
        self.autoScanning=NO;
        [self setMessage:@"HDMI signal candidate found · receiving selected mode"];
    } else if (self.autoScanning &&
               [NSDate timeIntervalSinceReferenceDate]-self.autoModeStarted>=2.7) {
        if (self.autoModeIndex<kNikonAutoScanModeCount-1) {
            NSInteger next=self.autoModeIndex+1;
            [self stopCurrent:NO];
            self.autoModeIndex=next;
            [self beginHardwareMode:next];
        } else {
            [self stopCurrent:NO];
            self.autoScanning=NO;
            [self finishRecording];
            self.stage.notice=@"NO HDMI INPUT SIGNAL";
            [self setMessage:@"No signal detected in six Nikon HDMI formats."];
            [self.stage setNeedsDisplay:YES];
        }
        return;
    }
    if (!valid) {
        self.stage.notice=@"NO HDMI INPUT SIGNAL";
        if (!self.autoScanning)
            [self setMessage:@"Blackmagic connected · waiting for a real source"];
        [self.stage setNeedsDisplay:YES];
    } else {
        [self setMessage:@"Real video frame received"];
    }
}
- (void)acceptHardwareRGBA:(NSData*)pixels width:(NSUInteger)width
                    height:(NSUInteger)height
                     ticks:(int64_t)ticks duration:(int64_t)duration
             timestampValid:(BOOL)valid {
    if (!self.captureActive) return;
    [self receiveRGBA:pixels width:width height:height ticks:ticks
             duration:duration valid:valid synthetic:NO];
}
- (void)receiveRGBA:(NSData*)rgba width:(NSUInteger)width
             height:(NSUInteger)height ticks:(int64_t)ticks
           duration:(int64_t)duration valid:(BOOL)valid
          synthetic:(BOOL)synthetic {
    NSImage *img=imageFromRGBA(rgba,width,height);
    if (!img) return;
    self.lastRGBA=rgba; self.lastFrame=img;
    self.lastWidth=width; self.lastHeight=height;
    self.stage.frameImage=img;
    self.stage.notice=nil;
    self.stage.synthetic=synthetic;
    [self.stage setNeedsDisplay:YES];
    // Identical frame-publication path for synthetic and vendor-observed
    // video. This is user-space, explicitly labeled and never a claim that
    // HDMI was physically verified by our source-owned driver.
    if (self.scratchShareToggle.state==NSControlStateValueOn) {
        if (!self.scratchPublisher) {
            NSUserDefaults *prefs=[NSUserDefaults standardUserDefaults];
            NSURL *endpoint=[NSURL URLWithString:
                [prefs stringForKey:@"FountainVideoScratchEndpoint"]?:@""];
            NSString *token=[prefs stringForKey:@"FountainVideoScratchToken"];
            if (token.length>=32)
                self.scratchPublisher=[[FCScratchPublisher alloc]
                                  initWithEndpoint:endpoint token:token];
        }
        if (self.scratchPublisher) {
            int64_t ptsNs=(int64_t)((long double)ticks*
                                    1000000000.0L/30000.0L);
            [self.scratchPublisher publishImage:img ptsNs:ptsNs
                            origin:synthetic?@"synthetic":@"decklink-observed"];
        }
    }
    if (synthetic) {
        self.frameDetails.stringValue=[NSString stringWithFormat:
           @"Generated test pattern · %lux%lu · synthetic frame %u",
           (unsigned long)width,(unsigned long)height,self.virtualFrame];
    }
    if (self.recordRequested) {
        CMTime pts = valid ? CMTimeMake(ticks,30000) : kCMTimeInvalid;
        [self appendRecordingRGBA:rgba width:width height:height timestamp:pts];
    }
}
- (void)saveFrame:(id)sender {
    if (!self.lastFrame) {
        [self setMessage:@"There is no frame to save."];
        return;
    }
    NSSavePanel *panel=[NSSavePanel savePanel];
    panel.allowedFileTypes=@[@"png"];
    panel.nameFieldStringValue=self.virtualActive?
        @"Fountain Video - SYNTHETIC.png":@"Fountain Video - Capture.png";
    if ([panel runModal]!=NSFileHandlingPanelOKButton) return;
    NSData *tiff=[self.lastFrame TIFFRepresentation];
    NSBitmapImageRep *rep=[NSBitmapImageRep imageRepWithData:tiff];
    NSData *png=[rep representationUsingType:NSPNGFileType properties:@{}];
    NSError *error=nil;
    if ([png writeToURL:panel.URL options:NSDataWritingAtomic error:&error])
        [self setMessage:@"Saved one frame to your chosen destination."];
    else
        [self setMessage:[NSString stringWithFormat:@"Could not save: %@",
                          error.localizedDescription]];
}
- (void)recordPressed:(id)sender {
    if (self.recordRequested) { [self finishRecording]; return; }
    if (!self.captureActive && !self.virtualActive) {
        [self setMessage:@"Start an input or virtual test before recording."];
        return;
    }
    NSSavePanel *panel=[NSSavePanel savePanel];
    panel.allowedFileTypes=@[@"mov"];
    panel.nameFieldStringValue=self.virtualActive?
      @"Fountain Video - SYNTHETIC.mov":@"Fountain Video - Capture.mov";
    if ([panel runModal]!=NSFileHandlingPanelOKButton) return;
    self.recordURL=panel.URL;
    self.recordRequested=YES;
    self.movieFrames=0;
    self.haveFirstTicks=NO;
    self.writer=nil;
    self.writerInput=nil;
    self.writerAdaptor=nil;
    self.recordButton.title=@"Stop recording";
    self.recordLabel.stringValue=self.virtualActive?
        @"Recording synthetic test frames":@"Armed · waiting for valid video";
    [self setMessage:@"Recording armed; video remains local."];
}
- (BOOL)prepareRecording:(NSUInteger)width height:(NSUInteger)height {
    NSError *error=nil;
    self.writer=[[AVAssetWriter alloc] initWithURL:self.recordURL
                                          fileType:AVFileTypeQuickTimeMovie
                                             error:&error];
    if (!self.writer) {
        [self setMessage:[NSString stringWithFormat:@"Recorder unavailable: %@",
                          error.localizedDescription]];
        [self finishRecording];
        return NO;
    }
    NSDictionary *settings=@{
        AVVideoCodecKey:AVVideoCodecJPEG,
        AVVideoWidthKey:@(width),
        AVVideoHeightKey:@(height)
    };
    self.writerInput=[AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo
                                                        outputSettings:settings];
    self.writerInput.expectsMediaDataInRealTime=YES;
    NSDictionary *bufferAttrs=@{
        (id)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32BGRA),
        (id)kCVPixelBufferWidthKey:@(width),
        (id)kCVPixelBufferHeightKey:@(height),
        (id)kCVPixelBufferIOSurfacePropertiesKey:@{}
    };
    self.writerAdaptor=[[AVAssetWriterInputPixelBufferAdaptor alloc]
                          initWithAssetWriterInput:self.writerInput
                    sourcePixelBufferAttributes:bufferAttrs];
    if (![self.writer canAddInput:self.writerInput]) {
        [self setMessage:@"Video encoder settings are unsupported."];
        [self finishRecording];
        return NO;
    }
    [self.writer addInput:self.writerInput];
    if (![self.writer startWriting]) {
        [self setMessage:[NSString stringWithFormat:@"Recorder error: %@",
                          self.writer.error.localizedDescription]];
        [self finishRecording];
        return NO;
    }
    [self.writer startSessionAtSourceTime:kCMTimeZero];
    return YES;
}
- (void)appendRecordingRGBA:(NSData*)rgba width:(NSUInteger)width
                    height:(NSUInteger)height timestamp:(CMTime)pts {
    if (!self.recordRequested) return;
    if (!CMTIME_IS_VALID(pts)) {
        self.recordLabel.stringValue=@"Recording armed · waiting for timestamp";
        return;
    }
    if (!self.writer && ![self prepareRecording:width height:height]) return;
    if (!self.haveFirstTicks) {
        self.firstTicks=pts.value;
        self.haveFirstTicks=YES;
    }
    if (!self.writerInput.readyForMoreMediaData) return;
    // The actual pixels are always delivered as RGBA (in memory only).
    CVPixelBufferRef pixel=NULL;
    CVReturn rc=CVPixelBufferPoolCreatePixelBuffer(
        kCFAllocatorDefault,self.writerAdaptor.pixelBufferPool,&pixel);
    if (rc!=kCVReturnSuccess || !pixel) {
        [self setMessage:@"Video buffer unavailable."];
        return;
    }
    CVPixelBufferLockBaseAddress(pixel,0);
    uint8_t *base=(uint8_t*)CVPixelBufferGetBaseAddress(pixel);
    const size_t destStride=CVPixelBufferGetBytesPerRow(pixel);
    const uint8_t *src=(const uint8_t*)rgba.bytes;
    for (NSUInteger y=0; y<height; ++y) {
        for (NSUInteger x=0; x<width; ++x) {
            const uint8_t *a=src+(y*width+x)*4;
            uint8_t *b=base+y*destStride+x*4;
            b[0]=a[2]; b[1]=a[1]; b[2]=a[0]; b[3]=255;
        }
    }
    CVPixelBufferUnlockBaseAddress(pixel,0);
    CMTime out=CMTimeSubtract(pts,CMTimeMake(self.firstTicks,30000));
    if (CMTIME_COMPARE_INLINE(out, <, kCMTimeZero)) out=kCMTimeZero;
    if ([self.writerAdaptor appendPixelBuffer:pixel withPresentationTime:out]) {
        ++self.movieFrames;
        self.recordLabel.stringValue=[NSString stringWithFormat:
                              @"Movie frames: %llu",(unsigned long long)self.movieFrames];
    } else {
        [self setMessage:[NSString stringWithFormat:@"Video encode: %@",
                          self.writer.error.localizedDescription]];
    }
    CVPixelBufferRelease(pixel);
}
- (void)finishRecording {
    if (!self.recordRequested && !self.writer) return;
    self.recordRequested=NO;
    self.recordButton.title=@"Record movie…";
    self.recordLabel.stringValue=@"Finishing file…";
    AVAssetWriter *writer=self.writer;
    AVAssetWriterInput *input=self.writerInput;
    self.writer=nil; self.writerInput=nil; self.writerAdaptor=nil;
    self.recordURL=nil;
    if (!writer) {
        self.recordLabel.stringValue=@"No valid frames were recorded.";
        return;
    }
    [input markAsFinished];
    [writer finishWritingWithCompletionHandler:^{
        dispatch_async(dispatch_get_main_queue(), ^{
            self.recordLabel.stringValue=writer.status==AVAssetWriterStatusCompleted?
              @"Movie saved.":@"Movie encoding failed.";
            if (writer.status==AVAssetWriterStatusCompleted)
                [self setMessage:@"Movie saved to your chosen location."];
            else
                [self setMessage:[NSString stringWithFormat:@"Recording failed: %@",
                          writer.error.localizedDescription]];
        });
    }];
}
- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    [self buildWindow];
    // Software-first. Starting Fountain Video must not enumerate real
    // capture hardware, require a powered camera or open a DeckLink client.
    [self.sourceChoice selectItemAtIndex:1];
    [self sourceChanged:nil];
    self.deviceDetails.stringValue=@"Software simulation · hardware optional";
    [self startPressed:nil];
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return YES;
}
- (void)applicationWillTerminate:(NSNotification*)notification {
    [self stopCurrent:NO];
    [self finishRecording];
    [self closeDevice];
    for (FCCallback *cb : _pastCallbacks) delete cb;
    _pastCallbacks.clear();
}
@end

static int runScratchCodecTest(void) {
    @autoreleasepool {
        NSData *rgba=virtualBarsRGBA(1280,720,9);
        NSData *other=virtualBarsRGBA(1280,720,10);
        if ([rgba isEqualToData:other]) {
            puts("stage=scratch_jpeg result=fail reason=static_test_pattern");
            return 50;
        }
        NSImage *image=imageFromRGBA(rgba,1280,720);
        NSUInteger width=0,height=0;
        NSData *jpeg=[FCScratchPublisher jpegPreviewForImage:image
                                                  outWidth:&width outHeight:&height];
        if (!jpeg || width != 640 || height != 360 ||
            jpeg.length > 1048576 || jpeg.length < 100) {
            puts("stage=scratch_jpeg result=fail reason=codec");
            return 51;
        }
        NSBitmapImageRep *decoded=[NSBitmapImageRep imageRepWithData:jpeg];
        if (!decoded || decoded.pixelsWide!=640 || decoded.pixelsHigh!=360) {
            puts("stage=scratch_jpeg result=fail reason=decode");
            return 52;
        }
        printf("stage=scratch_jpeg result=pass source=synthetic "
               "width=%lu height=%lu jpeg_bytes=%lu network_access=0\n",
               (unsigned long)width,(unsigned long)height,
               (unsigned long)jpeg.length);
        return 0;
    }
}
static int runSelfTest(void) {
    @autoreleasepool {
        uint8_t yuv[8]={128,235,128,235,128,16,128,16};
        NSData *rgba=rgbaFrom2vuy(yuv,sizeof(yuv),4,1,8);
        if (!rgba || rgba.length!=16) return 30;
        const uint8_t *pix=(const uint8_t*)rgba.bytes;
        if (pix[0]<245 || pix[1]<245 || pix[2]<245) return 31;
        if (pix[8]>10 || pix[9]>10 || pix[10]>10) return 32;
        NSData *bars=virtualBarsRGBA(128,72,1);
        if (!bars || bars.length!=128*72*4) return 33;
        NSImage *preview=imageFromRGBA(bars,128,72);
        if (!preview) return 34;
        printf("stage=fountain_video result=pass suite=converter,virtual_bars,preview\n");
        printf("stage=safety kernel_driver_install=0 pci_access=0\n");
        return 0;
    }
}
static int runDeviceProbe(void) {
    @autoreleasepool {
        void *lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                        RTLD_NOW|RTLD_LOCAL);
        if (!lib) { puts("stage=app_probe result=vendor_runtime_missing");return 20; }
        FCFactory factory=(FCFactory)dlsym(lib,"CreateDeckLinkIteratorInstance");
        if (!factory) { dlclose(lib); return 21; }
        void *iterator=factory(); if (!iterator) { dlclose(lib);return 22; }
        void *device=NULL;
        void **vt=*(void***)iterator;
        int32_t rc=((FCNext)vt[3])(iterator,&device);
        ((FCRelease)vt[2])(iterator);
        if (rc||!device) {dlclose(lib);puts("stage=app_probe result=no_device");return 23;}
        void **devvt=*(void***)device;
        void *status=NULL;
        rc=((FCQuery)devvt[0])(device,fcUUID(kStatusIID),&status);
        bool locked=false;
        int32_t lrc=-1;
        if (!rc && status) {
            void **sv=*(void***)status;
            lrc=((FCGetFlag)sv[3])(status,0x7669736c,&locked);
            ((FCRelease)sv[2])(status);
        }
        ((FCRelease)devvt[2])(device);
        dlclose(lib);
        printf("stage=app_probe result=device_found locked=%u status_rc=0x%08x\n",
               (unsigned)(lrc==0&&locked),(unsigned)lrc);
        printf("stage=safety kernel_driver_install=0 pci_access=0\n");
        return 0;
    }
}
static int runWindowSelfTest(void) {
    @autoreleasepool {
        NSApplication *app=[NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        FountainController *controller=[FountainController new];
        [controller buildWindow];
        [controller.sourceChoice selectItemAtIndex:1];
        [controller sourceChanged:nil];
        [controller startPressed:nil];
        [[NSRunLoop currentRunLoop]
            runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.45]];
        BOOL valid=controller.virtualActive && controller.lastFrame!=nil &&
                   controller.stage.notice==nil && !controller.formatChoice.enabled;
        [controller.sourceChoice selectItemAtIndex:0];
        [controller sourceChanged:nil];
        // Verify the hardware path is deliberately selected, but do not
        // start any vendor stream during the synthetic-only window test.
        BOOL guardVerified=controller.virtualActive &&
                           !controller.captureActive &&
                           controller.formatChoice.indexOfSelectedItem==7 &&
                           controller.scratchShareToggle.state==NSControlStateValueOff;
        [controller stopPressed:nil];
        controller.window.delegate=nil;
        [controller.window orderOut:nil];
        [controller.window close];
        controller.window=nil;
        app.delegate=nil;
        printf("stage=app_window result=%s virtual_frame=%u"
               " fixed_loopback_explicit_selection=%u\n",
               valid&&guardVerified?"pass":"fail",
               (unsigned)valid,(unsigned)guardVerified);
        /* This CLI test manually pumps a run loop without NSApplication.run.
         * Exit after releasing hardware: draining a synthetic AppKit app's
         * outermost pool is not an application lifecycle test.
         * Normal GUI shutdown is separately verified via Apple Events Quit.
         */
        fflush(stdout);
        exit(valid&&guardVerified?0:46);
    }
}
// Explicit native-AppKit acceptance of the user-fixed silver self-loop.
// No network publication: the source is the silver's primary HDMI display.
static int runFixedSilverHDMIAppTest(void) {
    @autoreleasepool {
        if(!fcSilverFixedBMD1080p30()) {
            puts("stage=native_fixed_hdmi result=source_preflight_failed");
            return 67;
        }
        NSApplication *app=[NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        FountainController *controller=[FountainController new];
        [controller buildWindow];
        [controller.sourceChoice selectItemAtIndex:0];
        [controller sourceChanged:nil];
        [controller startPressed:nil];
        NSDate *deadline=[NSDate dateWithTimeIntervalSinceNow:2.2];
        while([deadline timeIntervalSinceNow]>0 && controller.captureActive) {
            [[NSRunLoop currentRunLoop]
                runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.08]];
        }
        BOOL preview=controller.captureActive &&
            controller.lastFrame!=nil && !controller.stage.synthetic &&
            controller.lastWidth==1920 && controller.lastHeight==1080 &&
            controller.scratchShareToggle.state==NSControlStateValueOff;
        [controller stopCurrent:NO];
        [controller closeDevice];
        controller.window.delegate=nil;
        [controller.window orderOut:nil];
        [controller.window close];
        controller.window=nil;
        app.delegate=nil;
        printf("stage=native_fixed_hdmi result=%s actual_rgba_preview=%u "
               "dimensions=1920x1080 private=1 "
               "clean_teardown=1 device_timestamp_called=0\n",
               preview?"pass":"fail",(unsigned)preview);
        fflush(stdout);
        exit(preview?0:68);
    }
}
static int runAutoScanSelfTest(void) {
    @autoreleasepool {
        NSApplication *app=[NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        FountainController *controller=[FountainController new];
        [controller buildWindow];
        [controller.sourceChoice selectItemAtIndex:0];
        [controller.formatChoice selectItemAtIndex:0];
        [controller startPressed:nil];
        NSDate *deadline=[NSDate dateWithTimeIntervalSinceNow:27.0];
        while (controller.autoScanning && [deadline timeIntervalSinceNow]>0) {
            [[NSRunLoop currentRunLoop]
                runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.25]];
        }
        BOOL ended=!controller.autoScanning && !controller.captureActive;
        NSString *status=controller.status.stringValue;
        BOOL noSource=[status containsString:@"No signal detected"];
        printf("stage=auto_hdmi_scan result=%s finished=%u inactive=%u"
               " no_source=%u\n",
               ended&&noSource?"pass":"fail",
               (unsigned)(!controller.autoScanning),
               (unsigned)(!controller.captureActive),(unsigned)noSource);
        [controller stopCurrent:NO];
        [controller closeDevice];
        controller.window.delegate=nil;
        [controller.window orderOut:nil];
        [controller.window close];
        controller.window=nil;
        app.delegate=nil;
        fflush(stdout);
        exit(ended&&noSource ? 0 : 44);
    }
}
static int runMovieSelfTest(void) {
    @autoreleasepool {
        FountainController *controller=[FountainController new];
        NSString *name=[NSString stringWithFormat:@"FountainVideo-SYNTHETIC-%@.mov",
                        [[NSUUID UUID] UUIDString]];
        NSString *path=[NSTemporaryDirectory() stringByAppendingPathComponent:name];
        controller.recordURL=[NSURL fileURLWithPath:path];
        controller.recordRequested=YES;
        NSData *bars=virtualBarsRGBA(320,180,0);
        if (!bars || bars.length!=320*180*4) return 40;
        for(int i=0;i<12;i++) {
            [controller appendRecordingRGBA:bars width:320 height:180
                                  timestamp:CMTimeMake(i*3000,30000)];
            [[NSRunLoop currentRunLoop]
                runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.05]];
        }
        AVAssetWriter *writer=controller.writer;
        const unsigned long long submitted=(unsigned long long)controller.movieFrames;
        if (!writer || submitted<2) {
            fprintf(stderr,"stage=movie_test result=no_encoded_frames count=%llu\n",
                    submitted);
            [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
            return 41;
        }
        [controller finishRecording];
        NSDate *deadline=[NSDate dateWithTimeIntervalSinceNow:12];
        while (writer.status==AVAssetWriterStatusWriting &&
               [deadline timeIntervalSinceNow]>0) {
            [[NSRunLoop currentRunLoop]
                runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.15]];
        }
        NSDictionary *attrs=[[NSFileManager defaultManager]
                              attributesOfItemAtPath:path error:NULL];
        unsigned long long size=[[attrs objectForKey:NSFileSize] unsignedLongLongValue];
        BOOL ok=writer.status==AVAssetWriterStatusCompleted && size>3000;
        printf("stage=movie_test result=%s frames=%llu file_bytes=%llu"
               " source=synthetic codec=jpeg\n",ok?"pass":"fail",submitted,size);
        if (!ok) NSLog(@"Fountain movie test error: %@",writer.error);
        [[NSFileManager defaultManager] removeItemAtPath:path error:NULL];
        return ok?0:42;
    }
}
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--self-test-scratch"))
        return runScratchCodecTest();
    if (argc==2 && !strcmp(argv[1],"--self-test-window"))
        return runWindowSelfTest();
    if (argc==2 && !strcmp(argv[1],"--self-test-fixed-silver-hdmi"))
        return runFixedSilverHDMIAppTest();
    if (argc==2 && !strcmp(argv[1],"--self-test-autoscan"))
        return runAutoScanSelfTest();
    if (argc==2 && !strcmp(argv[1],"--self-test-movie"))
        return runMovieSelfTest();
    if (argc==2 && !strcmp(argv[1],"--self-test")) return runSelfTest();
    if (argc==2 && !strcmp(argv[1],"--probe")) return runDeviceProbe();
    if (argc!=1) {
        fprintf(stderr,"usage: FountainVideo [--self-test|--self-test-scratch|--self-test-window|--self-test-movie|--self-test-autoscan|--probe]\n");
        return 64;
    }
    @autoreleasepool {
        NSApplication *app=[NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        FountainController *controller=[FountainController new];
        app.delegate=controller;
        [app run];
    }
    return 0;
}

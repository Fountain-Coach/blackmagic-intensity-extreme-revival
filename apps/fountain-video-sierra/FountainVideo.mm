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
typedef int32_t (*FCFrameTime)(void*, int64_t*, int64_t*, int64_t);

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
static CFUUIDBytes fcUUID(const unsigned char *id) {
    CFUUIDBytes result; memcpy(&result, id, 16); return result;
}
static uint32_t modeCode(NSInteger choice) {
    switch (choice) {
        case 0: return 0x48703330u;  // 1080p30
        case 1: return 0x48703235u;  // 1080p25
        case 2: return 0x68703630u;  // 720p60
        case 3: return 0x68703530u;  // 720p50
        default: return 0;
    }
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
    for (NSUInteger y=0; y<height; ++y) {
        for (NSUInteger x=0; x<width; ++x) {
            NSUInteger idx = (x * 8) / width;
            uint8_t *pixel = out + (y*width+x)*4;
            pixel[0]=bars[idx][0]; pixel[1]=bars[idx][1];
            pixel[2]=bars[idx][2]; pixel[3]=255;
            if (y<9 && x>=((sequence%30)*width/30) &&
                x<((sequence%30)*width/30)+10) {
                pixel[0]=pixel[1]=pixel[2]=120;
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
    if (!frame) return 0;
    ++cb->frames;
    void **vt = *(void***)frame;
    const uint32_t flags = ((FCFrameFlags)vt[7])(frame);
    if (flags & kNoInputSource) { ++cb->noSource; return 0; }
    ++cb->valid;
    // Decimate preview only; all true callback frame counts are preserved.
    if (cb->sequence.fetch_add(1) % 3) return 0;
    long width=((FCFrameDimension)vt[3])(frame);
    long height=((FCFrameDimension)vt[4])(frame);
    long stride=((FCFrameDimension)vt[5])(frame);
    if (width<2 || height<1 || width>4096 || height>2160 ||
        stride < width*2 || stride>65536 || (width&1)) return 0;
    void* ptr=NULL;
    if (((FCFrameBytes)vt[8])(frame,&ptr) || !ptr) return 0;
    int64_t t=0,d=0;
    BOOL timingOK=((FCFrameTime)vt[11])(frame,&t,&d,30000)==0 && d>0;
    @autoreleasepool {
        NSData *rgba=rgbaFrom2vuy((const uint8_t*)ptr,
                                 (size_t)stride*(size_t)height,
                                 width,height,stride);
        if (!rgba) return 0;
        FountainController *owner=(__bridge FountainController*)cb->context;
        dispatch_async(dispatch_get_main_queue(), ^{
            [owner acceptHardwareRGBA:rgba width:(NSUInteger)width
                                 height:(NSUInteger)height ticks:t duration:d
                          timestampValid:timingOK];
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
    NSString *notice=self.notice?:@"NO SOURCE";
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
@property(nonatomic,strong) NSWindow *window;
@property(nonatomic,strong) VideoStage *stage;
@property(nonatomic,strong) NSTextField *status;
@property(nonatomic,strong) NSTextField *deviceDetails;
@property(nonatomic,strong) NSTextField *frameDetails;
@property(nonatomic,strong) NSTextField *recordLabel;
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
    [root addSubview:self.sourceChoice];
    [root addSubview:fcLabel(NSMakeRect(822,481,260,23),
                            @"Capture format",13,muted,NO)];
    self.formatChoice=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(818,441,276,32)
                                                 pullsDown:NO];
    [self.formatChoice addItemsWithTitles:@[@"1080p30",@"1080p25",
                                             @"720p60",@"720p50"]];
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
    NSButton *refresh=fcButton(NSMakeRect(818,50,276,36),
                         @"Refresh device status",self,@selector(refreshPressed:));
    [root addSubview:refresh];
    [root addSubview:fcLabel(NSMakeRect(28,32,755,58),
     @"Virtual video is labeled as synthetic. Real preview and recording use the existing\nDeckLink runtime. No MacBook source, kernel modification or firmware installation.",
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
    [self stopCurrent:NO];
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
    self.stage.synthetic=NO;
    _selectedMode=modeCode(self.formatChoice.indexOfSelectedItem);
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
    _callback->sequence.store(0); _callback->context=(__bridge void*)self;
    _pastCallbacks.push_back(_callback);
    void **inputTable=*(void***)_input;
    rc=((FCSetCallback)inputTable[17])(_input,_callback);
    if (rc) { [self setMessage:@"Cannot attach preview callback."];
              [self stopCurrent:NO]; return; }
    rc=((FCEnableInput)inputTable[6])(_input,_selectedMode,kPixel2vuy,0);
    if (rc) { [self setMessage:[NSString stringWithFormat:
                              @"Cannot enable input (0x%08x).",(unsigned)rc]];
              [self stopCurrent:NO]; return; }
    self.captureActive=YES;
    rc=((FCNoArg)inputTable[13])(_input);
    if (rc) {
        [self setMessage:[NSString stringWithFormat:
                            @"Cannot start capture (0x%08x).",(unsigned)rc]];
        [self stopCurrent:NO]; return;
    }
    self.stage.notice=@"WAITING FOR HDMI SIGNAL";
    [self setMessage:@"Capture running · checking for live frames"];
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
    if (!valid) {
        self.stage.notice=@"NO HDMI INPUT SIGNAL";
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
    [self refreshPressed:nil];
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
int main(int argc,char **argv) {
    if (argc==2 && !strcmp(argv[1],"--self-test")) return runSelfTest();
    if (argc==2 && !strcmp(argv[1],"--probe")) return runDeviceProbe();
    if (argc!=1) {
        fprintf(stderr,"usage: FountainVideo [--self-test|--probe]\n");
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

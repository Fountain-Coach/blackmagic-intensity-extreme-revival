// Fountain Window Tap, Sierra 10.12.6, original silver Macmini5,3.
// Capture an explicitly selected application WINDOW, optionally crop a rect
// within that window, and (only on explicit opt-in) publish to LOCAL Syphon.
// Does not touch Blackmagic, HDMI, device drivers, virtual machines or network.
// Source-specific, bounded, fail-closed: never capture the whole desktop by
// default; never auto-record private application pixels or auto-publish.
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#import <OpenGL/gl.h>
#import <OpenGL/OpenGL.h>
#import <Syphon/Syphon.h>
#import <CommonCrypto/CommonDigest.h>
#import <sys/sysctl.h>
#import <sys/stat.h>
#import <fcntl.h>
#import <unistd.h>
#include <stdlib.h>
#include <string.h>

static const NSUInteger kMaxSide=4096;
static NSString *const kPrivateDir=@"/Volumes/Fresh/FountainVideoDockerLab/private-window-tap";

static BOOL originalSilver(void) {
    char model[80]={0}; size_t sz=sizeof(model);
    if(sysctlbyname("hw.model",model,&sz,NULL,0)!=0 ||
       strcmp(model,"Macmini5,3")!=0)return NO;
    return [[NSProcessInfo processInfo].operatingSystemVersionString containsString:@"10.12.6"];
}
static BOOL numeric(const char *arg,NSUInteger *out,NSUInteger min,NSUInteger max) {
    if(!arg || !arg[0] || arg[0]=='-' || arg[0]=='+')return NO;
    char *end=NULL; unsigned long v=strtoul(arg,&end,10);
    if(!end||*end!='\0'||v<min||v>max)return NO;
    *out=(NSUInteger)v;return YES;
}
static NSDictionary *windowInfo(CGWindowID wid) {
    CFArrayRef array=CGWindowListCopyWindowInfo(kCGWindowListOptionIncludingWindow,wid);
    if(!array)return nil;
    NSArray *a=CFBridgingRelease(array);
    for(NSDictionary *info in a)
        if([info[(id)kCGWindowNumber] unsignedIntValue]==wid)return info;
    return nil;
}
static BOOL validWindow(CGWindowID wid,pid_t pid,BOOL requireVisible) {
    NSDictionary *i=windowInfo(wid);
    if(!i||[i[(id)kCGWindowOwnerPID] intValue]!=pid ||
       [i[(id)kCGWindowLayer] intValue]!=0)return NO;
    if(requireVisible && ![i[(id)kCGWindowIsOnscreen] boolValue])return NO;
    NSDictionary *bounds=i[(id)kCGWindowBounds];
    CGRect rect=CGRectZero;
    if(!CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)bounds,&rect))return NO;
    return rect.size.width>=8 && rect.size.height>=8 &&
           rect.size.width<=kMaxSide && rect.size.height<=kMaxSide;
}
static void listWindows(void) {
    CFArrayRef raw=CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly|
                                kCGWindowListExcludeDesktopElements,kCGNullWindowID);
    if(!raw){puts("stage=window_tap result=WINDOW_LIST_UNAVAILABLE");return;}
    NSArray *windows=CFBridgingRelease(raw);
    unsigned shown=0;
    for(NSDictionary *i in windows) {
        if([i[(id)kCGWindowLayer] intValue]!=0 || shown>=35)continue;
        pid_t p=[i[(id)kCGWindowOwnerPID] intValue];
        CGWindowID w=[i[(id)kCGWindowNumber] unsignedIntValue];
        NSDictionary *b=i[(id)kCGWindowBounds];CGRect rect=CGRectZero;
        if(!CGRectMakeWithDictionaryRepresentation((__bridge CFDictionaryRef)b,&rect)
           || rect.size.width<8 ||rect.size.height<8)continue;
        NSString *owner=i[(id)kCGWindowOwnerName]?:@"?";
        NSString *title=i[(id)kCGWindowName]?:@"";
        // Escape tabs and line breaks; listing is local and not a recording.
        title=[[title stringByReplacingOccurrencesOfString:@"\n" withString:@" "]
               stringByReplacingOccurrencesOfString:@"\t" withString:@" "];
        printf("stage=window_candidate id=%u pid=%d app=%s title=%s bounds=%.0fx%.0f\n",
               w,p,owner.UTF8String,title.UTF8String,rect.size.width,rect.size.height);
        ++shown;
    }
    printf("stage=window_list result=PASS eligible=%u pixels_saved=0 pixels_published=0\n",shown);
}
static void listSyphon(void) {
    SyphonServerDirectory *directory=[SyphonServerDirectory sharedDirectory];
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.5]];
    NSArray *servers=[directory servers];
    printf("stage=syphon_directory servers=%lu local_only=1\n",(unsigned long)servers.count);
    for(NSDictionary *s in servers)
        printf("stage=syphon_server app=%s name=%s\n",
               [s[SyphonServerDescriptionAppNameKey]?:@"" UTF8String],
               [s[SyphonServerDescriptionNameKey]?:@"" UTF8String]);
}
static CGRect parseRect(const char* raw,BOOL *ok) {
    CGRect rect=CGRectZero;*ok=NO;if(!raw)return rect;
    unsigned long x,y,w,h;char rest='\0';
    if(sscanf(raw,"%lu,%lu,%lu,%lu%c",&x,&y,&w,&h,&rest)!=4)return rect;
    if(x>kMaxSide||y>kMaxSide||w<1||h<1||w>kMaxSide||h>kMaxSide)return rect;
    *ok=YES;
    return CGRectMake((CGFloat)x,(CGFloat)y,(CGFloat)w,(CGFloat)h);
}
static CGImageRef selectedImage(CGWindowID wid,CGRect requested,BOOL crop) {
    CGImageRef original=CGWindowListCreateImage(CGRectNull,kCGWindowListOptionIncludingWindow,
                  wid,kCGWindowImageBoundsIgnoreFraming|kCGWindowImageNominalResolution);
    if(!original)return NULL;
    size_t w=CGImageGetWidth(original),h=CGImageGetHeight(original);
    if(w<1||h<1||w>kMaxSide||h>kMaxSide){CGImageRelease(original);return NULL;}
    if(!crop)return original;
    if(requested.origin.x<0 || requested.origin.y<0 ||
       CGRectGetMaxX(requested)>(CGFloat)w ||
       CGRectGetMaxY(requested)>(CGFloat)h) {
        CGImageRelease(original);return NULL;
    }
    CGImageRef sub=CGImageCreateWithImageInRect(original,requested);
    CGImageRelease(original);return sub;
}
static NSData* pixelsRGBA(CGImageRef image) {
    if(!image)return nil;
    size_t w=CGImageGetWidth(image),h=CGImageGetHeight(image);
    if(!w||!h||w>kMaxSide||h>kMaxSide)return nil;
    NSMutableData *b=[NSMutableData dataWithLength:w*h*4];
    if(!b)return nil;
    CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGContextRef c=CGBitmapContextCreate(b.mutableBytes,w,h,8,w*4,space,
                          kCGBitmapByteOrder32Big|kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(space);
    if(!c)return nil;
    CGContextDrawImage(c,CGRectMake(0,0,w,h),image);
    CGContextRelease(c);
    return b;
}
static NSString* digest(NSData *pixels) {
    unsigned char out[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(pixels.bytes,(CC_LONG)pixels.length,out);
    NSMutableString *hex=[NSMutableString string];
    for(int i=0;i<CC_SHA256_DIGEST_LENGTH;++i)[hex appendFormat:@"%02x",out[i]];
    return hex;
}
static BOOL privateFolder(void) {
    NSFileManager *fm=[NSFileManager defaultManager];
    BOOL isdir=NO;
    if(![fm fileExistsAtPath:kPrivateDir isDirectory:&isdir]) {
        if(![fm createDirectoryAtPath:kPrivateDir withIntermediateDirectories:NO
                 attributes:@{NSFilePosixPermissions:@0700} error:nil])return NO;
    } else if(!isdir)return NO;
    struct stat s;
    if(lstat(kPrivateDir.fileSystemRepresentation,&s)!=0 ||
       !S_ISDIR(s.st_mode)||s.st_uid!=getuid()||(s.st_mode&0777)!=0700)return NO;
    return YES;
}
static BOOL savePrivatePNG(CGImageRef image,NSString* basename) {
    if(!privateFolder() || [basename containsString:@"/"])return NO;
    NSString *full=[kPrivateDir stringByAppendingPathComponent:basename];
    int fd=open(full.fileSystemRepresentation,O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd<0)return NO;
    CGImageDestinationRef dest=NULL;
    CFMutableDataRef png=CFDataCreateMutable(kCFAllocatorDefault,0);
    if(png)dest=CGImageDestinationCreateWithData(png,CFSTR("public.png"),1,NULL);
    BOOL ok=NO;
    if(dest) {
        CGImageDestinationAddImage(dest,image,NULL);
        if(CGImageDestinationFinalize(dest)) {
            const UInt8* bytes=CFDataGetBytePtr(png);CFIndex sz=CFDataGetLength(png);
            CFIndex offset=0;
            while(offset<sz) {
                ssize_t n=write(fd,bytes+offset,(size_t)(sz-offset));
                if(n<=0)break;offset+=n;
            }
            ok=(offset==sz && sz>0);
        }
        CFRelease(dest);
    }
    if(png)CFRelease(png);
    close(fd);
    if(!ok)unlink(full.fileSystemRepresentation);
    return ok;
}
@interface TapFixtureView:NSView
@property(nonatomic,assign)NSUInteger phase;
@end
@implementation TapFixtureView
- (void)drawRect:(NSRect)dirty {
    (void)dirty;
    [[NSColor colorWithCalibratedRed:.1 green:.2 blue:.45 alpha:1] set];
    NSRectFill(self.bounds);
    [[NSColor whiteColor] set];
    for(NSUInteger x=16;x<self.bounds.size.width;x+=32)
        NSRectFill(NSMakeRect(x,0,1,self.bounds.size.height));
    [[NSColor colorWithCalibratedRed:.98 green:.55 blue:.10 alpha:1] set];
    NSRectFill(NSMakeRect(24+(self.phase%7)*23,70,85,85));
    NSString *s=[NSString stringWithFormat:@"FOUNTAIN WINDOW FIXTURE  %04lu",(unsigned long)self.phase];
    [s drawAtPoint:NSMakePoint(18,220)
        withAttributes:@{NSFontAttributeName:[NSFont boldSystemFontOfSize:19],
                         NSForegroundColorAttributeName:[NSColor whiteColor]}];
}
@end
static NSWindow* fixture(TapFixtureView **outView) {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    NSWindow *window=[[NSWindow alloc]
        initWithContentRect:NSMakeRect(250,220,640,360)
                  styleMask:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable)
                    backing:NSBackingStoreBuffered defer:NO];
    // NSWindow releaseWhenClosed defaults to YES on Sierra; ARC retains the
    // fixture through main. Disable self-release to prevent over-release when
    // the local autorelease pool drains after the bounded test.
    [window setReleasedWhenClosed:NO];
    window.title=@"Fountain Window Tap Test Fixture";
    TapFixtureView* view=[[TapFixtureView alloc] initWithFrame:NSMakeRect(0,0,640,335)];
    view.phase=0;window.contentView=view;*outView=view;
    [window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:NO];
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.6]];
    return window;
}
static BOOL makeGL(NSOpenGLContext **outGL) {
    NSOpenGLPixelFormatAttribute attrs[]={NSOpenGLPFAAccelerated,
                                           NSOpenGLPFAColorSize,24,0};
    NSOpenGLPixelFormat* format=[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    if(!format)return NO;
    NSOpenGLContext *ctx=[[NSOpenGLContext alloc] initWithFormat:format shareContext:nil];
    if(!ctx)return NO;[ctx makeCurrentContext];*outGL=ctx;return YES;
}
static BOOL publish(CGImageRef image,NSOpenGLContext* gl,SyphonServer *server) {
    NSData *rgba=pixelsRGBA(image);if(!rgba)return NO;
    size_t w=CGImageGetWidth(image),h=CGImageGetHeight(image);
    [gl makeCurrentContext];
    GLuint tex=0;glGenTextures(1,&tex);if(!tex)return NO;
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB,tex);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_RGBA,(GLsizei)w,(GLsizei)h,0,
                 GL_RGBA,GL_UNSIGNED_BYTE,rgba.bytes);
    GLenum error=glGetError();
    if(error==GL_NO_ERROR)
      [server publishFrameTexture:tex textureTarget:GL_TEXTURE_RECTANGLE_ARB
        imageRegion:NSMakeRect(0,0,w,h)
        textureDimensions:NSMakeSize(w,h) flipped:NO];
    glDeleteTextures(1,&tex);
    return error==GL_NO_ERROR;
}
static void usage(void) {
    puts("usage: fountain-window-tap --list | --list-syphon | --fixture-test | --fixture-publish");
    puts("  --probe --id WINDOW_ID --pid OWNER_PID [--rect x,y,w,h]");
    puts("  --snapshot --id WINDOW_ID --pid OWNER_PID [--rect x,y,w,h] --filename NAME.png");
    puts("  --publish --id WINDOW_ID --pid OWNER_PID [--rect x,y,w,h] --allow-local-syphon");
    puts("  Publication: at most 20 frames / 5 fps, local Syphon only. No network or desktop fallback.");
}
int main(int argc,const char**argv) {
    @autoreleasepool {
        if(!originalSilver()){puts("stage=window_tap result=WRONG_HOST");return 20;}
        if(argc==2 && strcmp(argv[1],"--list")==0){listWindows();return 0;}
        if(argc==2 && strcmp(argv[1],"--list-syphon")==0){listSyphon();return 0;}
        BOOL selfTest=argc==2 && strcmp(argv[1],"--fixture-test")==0;
        BOOL fixturePublish=argc==2 && strcmp(argv[1],"--fixture-publish")==0;
        BOOL probe=argc>1&&strcmp(argv[1],"--probe")==0;
        BOOL snap=argc>1&&strcmp(argv[1],"--snapshot")==0;
        BOOL publishing=argc>1&&strcmp(argv[1],"--publish")==0;
        if(!(selfTest||fixturePublish||probe||snap||publishing)){usage();return 64;}
        NSUInteger win=0,owner=0;
        BOOL hasWindow=NO,hasOwner=NO,hasRect=NO,allowPublish=NO;
        CGRect rect=CGRectZero;NSString *filename=nil;
        if(!(selfTest||fixturePublish)) {
            for(int i=2;i<argc;++i) {
                if(!strcmp(argv[i],"--id")&&i+1<argc)
                    hasWindow=numeric(argv[++i],&win,1,UINT32_MAX);
                else if(!strcmp(argv[i],"--pid")&&i+1<argc)
                    hasOwner=numeric(argv[++i],&owner,1,INT32_MAX);
                else if(!strcmp(argv[i],"--rect")&&i+1<argc)
                    rect=parseRect(argv[++i],&hasRect);
                else if(!strcmp(argv[i],"--filename")&&i+1<argc)
                    filename=[NSString stringWithUTF8String:argv[++i]];
                else if(!strcmp(argv[i],"--allow-local-syphon"))
                    allowPublish=YES;
                else {usage();return 64;}
            }
            if(!hasWindow||!hasOwner || (publishing&&!allowPublish) ||
               (snap && (filename.length<5||![filename hasSuffix:@".png"])))
                {puts("stage=window_tap result=EXPLICIT_SELECTION_REQUIRED");return 65;}
        }
        TapFixtureView *view=nil;
        NSWindow *testWindow=nil;
        if(selfTest||fixturePublish) {
            testWindow=fixture(&view);
            win=(NSUInteger)testWindow.windowNumber;owner=(NSUInteger)getpid();
            printf("stage=window_fixture owner_pid=%lu window_id=%lu\n",
                  (unsigned long)owner,(unsigned long)win);
        }
        if(!validWindow((CGWindowID)win,(pid_t)owner,YES)) {
            puts("stage=window_tap result=WINDOW_IDENTITY_NOT_VISIBLE");
            return 66;
        }
        NSOpenGLContext *gl=nil;SyphonServer *server=nil;
        if(fixturePublish||publishing) {
            if(!makeGL(&gl)){puts("stage=window_tap result=GL_NOT_AVAILABLE");return 67;}
            server=[[SyphonServer alloc] initWithName:@"Fountain Window Tap"
                                context:[gl CGLContextObj] options:nil];
            if(!server){puts("stage=window_tap result=SYPHON_SERVER_NOT_AVAILABLE");return 68;}
            puts("stage=window_tap syphon_server=Fountain_Window_Tap transport=local_only");
        }
        unsigned total=fixturePublish?20:publishing?20:selfTest?2:1;
        NSString *firstHash=nil,*lastHash=nil;
        unsigned okFrames=0;
        for(unsigned frame=0;frame<total;++frame) {
            if(view) {
                view.phase=frame*3;
                [view setNeedsDisplay:YES];
                [view displayIfNeeded];
            }
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.18]];
            if(!validWindow((CGWindowID)win,(pid_t)owner,YES)){
                puts("stage=window_tap result=WINDOW_CHANGED_OR_DISAPPEARED");break;
            }
            CGImageRef image=selectedImage((CGWindowID)win,rect,hasRect);
            NSData *rgba=pixelsRGBA(image);
            if(!image||!rgba) {
                if(image)CGImageRelease(image);
                puts("stage=window_tap result=IMAGE_UNAVAILABLE_OR_RECT_OUT_OF_BOUNDS");
                break;
            }
            NSString *hash=digest(rgba);
            if(!firstHash)firstHash=hash;lastHash=hash;
            BOOL acceptable=YES;
            // Synthetic smoke testing is deliberately memory-only. The
            // window/frame hash changes are the witness; no screenshots saved.
            // In particular never collide with private fixture-0/1 PNGs left
            // by an older run.
            if(selfTest)acceptable=YES;
            if(snap)acceptable=savePrivatePNG(image,filename);
            if(server)acceptable=publish(image,gl,server);
            printf("stage=window_frame seq=%u source_pid=%lu window_id=%lu "
                   "width=%zu height=%zu crop=%u sha256=%s syphon_published=%u disk_saved=%u\n",
                   frame,(unsigned long)owner,(unsigned long)win,
                   CGImageGetWidth(image),CGImageGetHeight(image),hasRect?1:0,
                   hash.UTF8String,(server&&acceptable)?1:0,
                   (snap&&acceptable)?1:0);
            CGImageRelease(image);
            if(!acceptable)break;
            ++okFrames;
            if(server)[[NSRunLoop currentRunLoop]
                   runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.22]];
        }
        [server stop];
        if(testWindow)[testWindow close];
        if(selfTest && okFrames==2 && ![firstHash isEqualToString:lastHash]) {
            puts("stage=fixture_acceptance result=PASS distinct_frames=2 private_pngs=0 "
                 "blackmagic_touched=0 desktop_captured=0 network=0");
            return 0;
        }
        if(okFrames==total) {
            printf("stage=window_tap result=PASS frames=%u syphon=%u fixture=%u\n",
                   okFrames,server?1:0,view?1:0);
            return 0;
        }
        printf("stage=window_tap result=FAIL accepted=%u requested=%u\n",okFrames,total);
        return 70;
    }
}

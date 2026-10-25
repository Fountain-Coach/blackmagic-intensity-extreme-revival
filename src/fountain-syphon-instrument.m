/*
 * Fountain Coach / DIE MASCHINE
 * Native Syphon instrument, generated and published without Black Syphon.
 *
 * Build on Sierra:
 *  clang -fobjc-arc -Wall -Wextra -Werror -std=gnu11 -I src \
 *      src/fountain-syphon-instrument.m -F SYHON_PARENT -framework Syphon \
 *      -framework Cocoa -framework OpenGL -o FountainVideoInstrument \
 *      -Wl,-rpath,@executable_path/../Frameworks
 * Bundle a local copy of Syphon.framework and its license in Contents/Frameworks.
 *
 * Local window is an independent preview. Video fields are produced from
 * fountain-video-score.h and no physical capture APIs are opened.
 */
#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glext.h>
#import <Syphon/Syphon.h>
#import "fountain-video-score.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { kWidth=960, kHeight=540, kFramesPerSecond=25 };
@interface FountainInstrument : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (strong) NSWindow *window;
@property (strong) NSOpenGLView *view;
@property (strong) NSOpenGLContext *context;
@property (strong) SyphonServer *server;
@property (strong) NSTimer *timer;
@property (assign) GLuint texture;
@property (assign) uint8_t *pixels;
@property (assign) uint32_t frame;
@property (assign) uint32_t published;
@property (assign) uint32_t lastCRC;
@property (assign) NSUInteger maxFrames;
@end

@implementation FountainInstrument
- (void)applicationDidFinishLaunching:(NSNotification *)note {
    (void)note;
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAAccelerated, NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize, (NSOpenGLPixelFormatAttribute)24,
        (NSOpenGLPixelFormatAttribute)0
    };
    NSOpenGLPixelFormat *format =
        [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    if (!format) {
        puts("stage=syphon result=no_accelerated_opengl_format");
        fflush(stdout);
        [NSApp terminate:nil];
        return;
    }
    NSRect rect=NSMakeRect(0,0,960,540);
    self.window=[[NSWindow alloc] initWithContentRect:rect
        styleMask:(NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|
                   NSWindowStyleMaskMiniaturizable)
        backing:NSBackingStoreBuffered defer:NO];
    [self.window setTitle:@"Fountain Video Instrument | Die Maschine"];
    self.view=[[NSOpenGLView alloc] initWithFrame:rect pixelFormat:format];
    [self.window setContentView:self.view];
    [self.window setDelegate:self];
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [self.view prepareOpenGL];
    self.context=[self.view openGLContext];
    [self.context makeCurrentContext];
    if (![self.context CGLContextObj]) {
        puts("stage=syphon result=no_opengl_context");
        fflush(stdout);
        [NSApp terminate:nil];
        return;
    }
    self.pixels=(uint8_t *)malloc((size_t)kWidth*kHeight*4);
    if (!self.pixels) {
        puts("stage=syphon result=allocation_failed");
        fflush(stdout);
        [NSApp terminate:nil];
        return;
    }
    glGenTextures(1,&_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB,self.texture);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_RGBA8,kWidth,kHeight,
                 0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    const GLenum glerr=glGetError();
    if(glerr!=GL_NO_ERROR){
        printf("stage=syphon result=texture_error gl=0x%x\n",(unsigned)glerr);
        fflush(stdout);
        [NSApp terminate:nil];
        return;
    }
    self.server=[[SyphonServer alloc] initWithName:@"Fountain Die Maschine"
               context:[self.context CGLContextObj] options:nil];
    if (!self.server) {
        puts("stage=syphon result=server_creation_failed");
        fflush(stdout);
        [NSApp terminate:nil];
        return;
    }
    printf("stage=syphon result=server_started name=Fountain_Die_Maschine"
           " size=%dx%d fps=%d source=generated not_hardware_capture=1\n",
           kWidth,kHeight,kFramesPerSecond);
    fflush(stdout);
    self.timer=[NSTimer scheduledTimerWithTimeInterval:(1.0/kFramesPerSecond)
                   target:self selector:@selector(renderTick:)
                   userInfo:nil repeats:YES];
    [self renderTick:nil];
}
- (void)renderTick:(NSTimer *)timer {
    (void)timer;
    if(!self.pixels||!self.server)return;
    if(fc_video_score_rgba(self.pixels,(size_t)kWidth*kHeight*4,
                           kWidth,kHeight,self.frame)!=0){
        puts("stage=syphon result=render_failed");
        fflush(stdout);
        [NSApp terminate:nil];
        return;
    }
    self.lastCRC=fc_video_score_crc(self.pixels,(size_t)kWidth*kHeight*4);
    [self.context makeCurrentContext];
    GLenum prior=glGetError();
    if(prior!=GL_NO_ERROR){
        printf("stage=syphon prior_gl_error=0x%x frame=%u\n",
               (unsigned)prior,(unsigned)self.frame);
        fflush(stdout);
    }
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB,self.texture);
    GLenum bindError=glGetError();
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,kWidth,kHeight,
                    GL_RGBA,GL_UNSIGNED_BYTE,self.pixels);
    GLenum updateError=glGetError();
    if(bindError!=GL_NO_ERROR || updateError!=GL_NO_ERROR){
        printf("stage=syphon result=texture_upload_failed"
               " frame=%u bind_error=0x%x upload_error=0x%x\n",
               (unsigned)self.frame,(unsigned)bindError,(unsigned)updateError);
        fflush(stdout);
        [NSApp terminate:nil];
        return;
    }
    [self.server publishFrameTexture:self.texture
          textureTarget:GL_TEXTURE_RECTANGLE_ARB
          imageRegion:NSMakeRect(0,0,kWidth,kHeight)
          textureDimensions:NSMakeSize(kWidth,kHeight) flipped:YES];
    /* GPU preview via the same published OpenGL texture. */
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
    glViewport(0,0,kWidth,kHeight);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_RECTANGLE_ARB);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0,kWidth,kHeight,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    glColor4f(1,1,1,1);
    glBegin(GL_QUADS);
      glTexCoord2f(0,0);glVertex2f(0,0);
      glTexCoord2f(kWidth,0);glVertex2f(kWidth,0);
      glTexCoord2f(kWidth,kHeight);glVertex2f(kWidth,kHeight);
      glTexCoord2f(0,kHeight);glVertex2f(0,kHeight);
    glEnd();
    [self.context flushBuffer];
    self.published++;
    if((self.published%25u)==0u) {
        printf("stage=syphon_frame count=%u score_frame=%u crc32like=0x%08x"
               " has_clients=%d source=generated\n",
               (unsigned)self.published,(unsigned)self.frame,
               (unsigned)self.lastCRC,(int)[self.server hasClients]);
        fflush(stdout);
    }
    self.frame++;
    if(self.maxFrames>0 && self.published>=self.maxFrames){
        printf("stage=syphon result=bounded_complete submitted=%u\n",
               (unsigned)self.published);
        fflush(stdout);
        [NSApp terminate:nil];
    }
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;return YES;
}
- (void)windowWillClose:(NSNotification *)note {
    (void)note;
    [NSApp terminate:nil];
}
- (void)applicationWillTerminate:(NSNotification *)note {
    (void)note;
    [self.timer invalidate];
    self.timer=nil;
    if(self.server){[self.server stop];self.server=nil;}
    if(self.context)[self.context makeCurrentContext];
    if(self.texture){glDeleteTextures(1,&_texture);self.texture=0;}
    if(self.pixels){free(self.pixels);self.pixels=NULL;}
    printf("stage=syphon result=stopped published=%u"
           " synthetic_only=1 capture_calls=0\n",(unsigned)self.published);
    fflush(stdout);
}
@end

int main(int argc,const char *argv[]) {
    @autoreleasepool {
        NSUInteger frames=0;
        for(int i=1;i<argc;i++){
            if(!strcmp(argv[i],"--frames")&&i+1<argc){
                char *end=NULL;
                unsigned long n=strtoul(argv[++i],&end,10);
                if(!end||*end||n<1||n>50000)return 64;
                frames=(NSUInteger)n;
            }else if(!strcmp(argv[i],"--live")){
                frames=0;
            }else {
                puts("usage: FountainVideoInstrument --frames 125|--live");
                return 64;
            }
        }
        NSApplication *app=[NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        FountainInstrument *engine=[[FountainInstrument alloc] init];
        engine.maxFrames=frames;
        [app setDelegate:engine];
        printf("stage=instrument starting=1 max_frames=%lu"
               " independent_of_black_syphon_application=1\n",
               (unsigned long)frames);
        fflush(stdout);
        [app run];
        return 0;
    }
}

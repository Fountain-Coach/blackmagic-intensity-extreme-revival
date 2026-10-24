// Independent Sierra Syphon client acceptance probe.
// Receives ONLY the explicitly labeled synthetic Fountain Window Tap Test
// server and independently reads real Syphon GPU textures back to CPU memory.
// Never accepts a general app/window server, captures a desktop, saves bytes,
// binds a network port, or touches Blackmagic/driver/HDMI/Docker.
#import <Cocoa/Cocoa.h>
#import <Syphon/Syphon.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <CommonCrypto/CommonDigest.h>
#import <sys/sysctl.h>
#import <unistd.h>
#include <stdio.h>
#include <string.h>

static BOOL correctHost(void) {
    char model[60]={0};size_t cap=sizeof(model);
    return sysctlbyname("hw.model",model,&cap,NULL,0)==0 &&
           strcmp(model,"Macmini5,3")==0 &&
           [[NSProcessInfo processInfo].operatingSystemVersionString containsString:@"10.12.6"];
}
static NSString *hashPixels(const unsigned char *p,size_t n) {
    unsigned char h[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(p,(CC_LONG)n,h);
    NSMutableString *result=[NSMutableString stringWithCapacity:64];
    for(int i=0;i<CC_SHA256_DIGEST_LENGTH;++i)[result appendFormat:@"%02x",h[i]];
    return result;
}
int main(int argc,const char **argv) {
    @autoreleasepool {
        if(argc!=2||strcmp(argv[1],"--fixture-receive")!=0)return 64;
        if(!correctHost()){puts("stage=syphon_receive result=WRONG_HOST");return 65;}
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
        NSOpenGLPixelFormatAttribute attrs[]={
           NSOpenGLPFAAccelerated,NSOpenGLPFAColorSize,24,0};
        NSOpenGLPixelFormat *pf=[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
        NSOpenGLContext *gl=pf?[[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil]:nil;
        if(!gl){puts("stage=syphon_receive result=NO_OPENGL_CONTEXT");return 66;}
        [gl makeCurrentContext];
        SyphonClient *client=nil;
        NSMutableSet *unique=[NSMutableSet set];
        unsigned received=0,invalid=0;
        NSDate *until=[NSDate dateWithTimeIntervalSinceNow:14.0];
        while([until timeIntervalSinceNow]>0 && unique.count<3) {
            @autoreleasepool {
                if(!client) {
                    NSArray *arr=[[SyphonServerDirectory sharedDirectory]
                          serversMatchingName:@"Fountain Window Tap Test" appName:nil];
                    if(arr.count>0) {
                        client=[[SyphonClient alloc] initWithServerDescription:arr[0]
                                              options:nil newFrameHandler:nil];
                        if(!client.isValid)client=nil;
                        if(client)puts("stage=syphon_receive server=Fountain_Window_Tap_Test valid=1");
                    }
                }
                if(client && client.isValid && client.hasNewFrame) {
                    [gl makeCurrentContext];
                    SyphonImage *image=[client newFrameImageForContext:[gl CGLContextObj]];
                    if(image) {
                        NSSize sz=image.textureSize;
                        size_t w=(size_t)sz.width,h=(size_t)sz.height;
                        if(w>0&&h>0&&w<=4096&&h<=4096&&image.textureName>0) {
                            NSMutableData *pixels=[NSMutableData dataWithLength:w*h*4];
                            glBindTexture(GL_TEXTURE_RECTANGLE_ARB,image.textureName);
                            glPixelStorei(GL_PACK_ALIGNMENT,1);
                            while(glGetError()!=GL_NO_ERROR) {} // flush old error
                            glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RGBA,GL_UNSIGNED_BYTE,
                                          pixels.mutableBytes);
                            GLenum err=glGetError();
                            if(err==GL_NO_ERROR) {
                                NSString *sha=hashPixels(pixels.bytes,pixels.length);
                                [unique addObject:sha];
                                ++received;
                                printf("stage=syphon_receive frame=%u size=%zux%zu sha256=%s distinct=%lu\n",
                                       received,w,h,sha.UTF8String,(unsigned long)unique.count);
                                fflush(stdout);
                            } else {
                                ++invalid;
                                printf("stage=syphon_receive result=GPU_READ_ERROR code=0x%x\n",err);
                            }
                        } else {
                            ++invalid;
                            puts("stage=syphon_receive result=BAD_TEXTURE_SIZE");
                        }
                    }
                }
            }
            [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.08]];
        }
        if(client)[client stop];
        printf("stage=syphon_receive result=%s textures_received=%u "
               "unique_gpu_readbacks=%lu gpu_read_errors=%u "
               "public_pixels=0 disk_saved=0 blackmagic_touched=0\n",
               received>=3&&unique.count>=2&&invalid==0?"PASS":"FAIL",
               received,(unsigned long)unique.count,invalid);
        return received>=3&&unique.count>=2&&invalid==0?0:70;
    }
}

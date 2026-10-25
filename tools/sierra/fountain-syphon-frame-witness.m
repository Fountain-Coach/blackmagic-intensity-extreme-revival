/*
 * Fountain Coach: independent Syphon real-pixel witness for Die Maschine.
 *
 * This tool is NOT Black Syphon. Uses only the installed Syphon framework
 * API and independent OpenGL context to verify changing image pixels across
 * the interprocess server/client boundary. Does not touch video hardware.
 *
 * clang -fobjc-arc -Wall -Wextra -Werror -fblocks -std=gnu11
 *       -F /Applications/Syphon Recorder.app/Contents/Frameworks
 *       -framework Syphon -framework Cocoa -framework OpenGL
 *       -Wl,-rpath,@executable_path/../Frameworks
 *
 * Copyright (c) Fountain Coach contributors, 2026. MIT.
 */
#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <Syphon/Syphon.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint32_t checksum(const uint8_t *p,size_t count) {
    uint32_t hash=2166136261u;
    for(size_t i=0;i<count;i++) hash=(hash^p[i])*16777619u;
    return hash;
}
int main(void){
    @autoreleasepool{
        SyphonServerDirectory *directory=[SyphonServerDirectory sharedDirectory];
        NSDate *deadline=[NSDate dateWithTimeIntervalSinceNow:3.0];
        NSDictionary *source=nil;
        while([deadline timeIntervalSinceNow]>0 && !source) {
            NSArray *servers=[directory servers];
            for(NSDictionary *entry in servers){
                NSString *name=[entry objectForKey:SyphonServerDescriptionNameKey];
                if([name isEqualToString:@"Fountain Die Maschine"]){
                    source=entry;break;
                }
            }
            if(!source)[[NSRunLoop currentRunLoop]
                 runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.12]];
        }
        if(!source){
            puts("stage=syphon_client result=no_fountain_server");
            return 20;
        }
        NSOpenGLPixelFormatAttribute attributes[]={
           NSOpenGLPFAAccelerated, NSOpenGLPFAColorSize,
           (NSOpenGLPixelFormatAttribute)24,
           (NSOpenGLPixelFormatAttribute)0
        };
        NSOpenGLPixelFormat *format=
          [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
        if(!format){puts("stage=syphon_client result=no_gl_format");return 21;}
        NSOpenGLContext *context=[[NSOpenGLContext alloc]
            initWithFormat:format shareContext:nil];
        if(!context){puts("stage=syphon_client result=no_gl_context");return 22;}
        [context makeCurrentContext];
        SyphonClient *client=[[SyphonClient alloc]
            initWithServerDescription:source options:nil newFrameHandler:nil];
        if(!client||![client isValid]){
            puts("stage=syphon_client result=client_not_connected");
            return 23;
        }
        printf("stage=syphon_client result=connected source=Fountain_Die_Maschine\n");
        fflush(stdout);
        uint32_t seen[2]={0,0};
        for(int i=0;i<2;i++){
            [[NSRunLoop currentRunLoop] runUntilDate:
               [NSDate dateWithTimeIntervalSinceNow:0.45]];
            [context makeCurrentContext];
            SyphonImage *frame=[client newFrameImageForContext:[context CGLContextObj]];
            if(!frame){puts("stage=syphon_client result=no_frame");return 24;}
            NSSize size=[frame textureSize];
            int width=(int)size.width,height=(int)size.height;
            if(width<2 || height<2 || width>4096 || height>2160){
                puts("stage=syphon_client result=unexpected_size");
                return 25;
            }
            size_t count=(size_t)width*(size_t)height*4u;
            uint8_t *pixels=(uint8_t*)malloc(count);
            if(!pixels)return 26;
            // Syphon's interprocess IOSurface texture can be sampled as
            // a render target; glGetTexImage is not a reliable IOSurface
            // readback method on Sierra Intel HD3000.
            while(glGetError()!=GL_NO_ERROR){}
            GLuint fbo=0;
            glGenFramebuffersEXT(1,&fbo);
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
            glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_RECTANGLE_ARB,
                [frame textureName],0);
            GLenum fbStatus=glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
            if(fbStatus!=GL_FRAMEBUFFER_COMPLETE_EXT){
                printf("stage=syphon_client result=incomplete_framebuffer"
                       " fb_status=0x%x\n",(unsigned)fbStatus);
                glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
                glDeleteFramebuffersEXT(1,&fbo);
                free(pixels);return 27;
            }
            glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
            glPixelStorei(GL_PACK_ALIGNMENT,1);
            glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
            GLenum err=glGetError();
            glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,0);
            glDeleteFramebuffersEXT(1,&fbo);
            if(err!=GL_NO_ERROR){
                printf("stage=syphon_client result=readback_failed"
                       " gl_error=0x%x\n",(unsigned)err);
                free(pixels);return 27;
            }
            uint32_t nonzero=0;
            for(size_t j=0;j<count;j+=97)if(pixels[j])nonzero++;
            seen[i]=checksum(pixels,count);
            printf("stage=syphon_frame sample=%d pixels=%dx%d crc=0x%08x"
                   " sampled_nonzero=%u\n",i+1,width,height,
                   (unsigned)seen[i],(unsigned)nonzero);
            fflush(stdout);
            free(pixels);
        }
        [client stop];
        printf("stage=syphon_client result=%s"
               " real_pixels_read=1 frame_variation=%d capture_source=generated\n",
               seen[0]!=seen[1]?"PASS":"NO_CHANGE",seen[0]!=seen[1]);
        return seen[0]!=seen[1]?0:28;
    }
}

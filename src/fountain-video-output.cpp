// Fountain Coach: physical HDMI playback of the owned Die Maschine score.
// The current Intensity Extreme remains attached by original Thunderbolt.
// This test leaves Sierra HDMI OUT -> Blackmagic HDMI IN untouched and does
// not enable the Blackmagic capture interface. HDMI OUT -> Panasonic is the
// physical monitor under test. No other capture/playback app is required.
//
// Adapter is deliberately separable from our pure C video-score.h. It calls
// the original Sierra DeckLink API today; this is NOT an independent driver.
//
// 720p60 and 1080p25 verified as original hardware output capabilities.
// Use a bounded experiment before any long-lived, system-managed playback.
// Copyright (c) Fountain Coach contributors, 2026. MIT.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "fountain-video-score.h"

typedef int32_t (*Query)(void*,CFUUIDBytes,void**);
typedef uint32_t (*Release)(void*);
typedef int32_t (*Next)(void*,void**);
typedef int32_t (*Enable)(void*,uint32_t,uint32_t);
typedef int32_t (*Disable)(void*);
typedef int32_t (*CreateFrame)(void*,int32_t,int32_t,int32_t,uint32_t,uint32_t,void**);
typedef int32_t (*GetBytes)(void*,void**);
typedef int32_t (*DisplaySync)(void*,void*);
typedef void* (*Factory)(void);

static const uint8_t outputIID[16] = {
   0xcc,0x5c,0x8a,0x6e,0x3f,0x2f,0x4b,0x3a,
   0x87,0xea,0xfd,0x78,0xaf,0x30,0x05,0x64};
static volatile sig_atomic_t stop_requested=0;
static void stop_handler(int x){(void)x;stop_requested=1;}
static void release_obj(void* p){
    if(p)((Release)(*(void***)p)[2])(p);
}
static double now_seconds(void){
    struct timeval t; gettimeofday(&t,NULL);
    return (double)t.tv_sec+(double)t.tv_usec/1000000.0;
}
int main(int argc,char**argv){
    if(argc!=4||strcmp(argv[1],"--mode")||
       (strcmp(argv[2],"720p60")&&strcmp(argv[2],"1080p25"))||
       strcmp(argv[3],"--eight-second-score")){
        fprintf(stderr,"usage: fountain-video-output --mode 720p60|1080p25 --eight-second-score\n");
        return 64;
    }
    const bool hd=(strcmp(argv[2],"720p60")!=0);
    const int width=hd?1920:1280,height=hd?1080:720,rate=hd?25:60;
    const uint32_t code=hd?0x48703235u:0x68703630u;
    const size_t bytes=(size_t)width*(size_t)height*2u;
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                    RTLD_NOW|RTLD_LOCAL);
    if(!lib){puts("stage=output result=original_framework_unavailable");return 20;}
    Factory factory=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    void* iter=factory?factory():NULL;
    if(!iter){dlclose(lib);puts("stage=output result=iterator_unavailable");return 21;}
    void* device=NULL;
    int32_t rc=((Next)(*(void***)iter)[3])(iter,&device);
    release_obj(iter);
    if(rc||!device){
        printf("stage=output result=no_physical_device rc=0x%08x\n",(unsigned)rc);
        dlclose(lib);return 22;
    }
    CFUUIDBytes iid;memcpy(&iid,outputIID,16);
    void* out=NULL;
    rc=((Query)(*(void***)device)[0])(device,iid,&out);
    if(rc||!out){
        printf("stage=output result=no_output_interface rc=0x%08x\n",(unsigned)rc);
        release_obj(device);dlclose(lib);return 23;
    }
    signal(SIGINT,stop_handler);signal(SIGTERM,stop_handler);
    bool active=false;
    void* frame=NULL;
    int status=0;
    const double start=now_seconds();
    rc=((Enable)(*(void***)out)[6])(out,code,0);
    printf("stage=output_enable mode=%s size=%dx%d fps=%d rc=0x%08x\n",
           argv[2],width,height,rate,(unsigned)rc);
    fflush(stdout);
    if(rc){status=24;goto finish;}
    active=true;
    rc=((CreateFrame)(*(void***)out)[9])(
        out,width,height,width*2,0x32767579u,0,&frame);
    printf("stage=frame_create rc=0x%08x width=%d height=%d bytes=%lu\n",
           (unsigned)rc,width,height,(unsigned long)bytes);
    fflush(stdout);
    if(rc||!frame){status=25;goto finish;}
    {
        void* raw=NULL;
        rc=((GetBytes)(*(void***)frame)[8])(frame,&raw);
        if(rc||!raw){status=26;goto finish;}
        uint8_t* pixels=(uint8_t*)raw;
        const int total=rate*8;
        const int update_interval=hd?3:6; // Animate at ~8-10 poses/sec
        uint32_t last_hash=0;
        int submitted=0,unique_updates=0;
        for(int n=0;n<total&&!stop_requested;n++){
            if((n%update_interval)==0){
                if(fc_video_score_uyvy(pixels,bytes,width,height,(uint32_t)n)){
                    status=27;break;
                }
                last_hash=fc_video_score_crc(pixels,bytes);
                unique_updates++;
            }
            rc=((DisplaySync)(*(void***)out)[11])(out,frame);
            if(rc){
                printf("stage=frame_send result=error n=%d rc=0x%08x\n",
                       n,(unsigned)rc);
                status=28;break;
            }
            submitted++;
            if((n%rate)==(rate-1)){
                printf("stage=physical_score progress_sec=%d"
                       " frame_submissions=%d generated_poses=%d"
                       " frame_crc=0x%08x clock_source=host\n",
                       (n+1)/rate,submitted,unique_updates,(unsigned)last_hash);
                fflush(stdout);
            }
            // Bounded dispatch pacing. The legacy synchronous API may block
            // internally; avoid intentionally submitting too fast.
            const double expected=start+(double)(n+1)/(double)rate;
            const double remain=expected-now_seconds();
            if(remain>0 && remain<1.0)usleep((useconds_t)(remain*1000000));
        }
        printf("stage=physical_score result=%s mode=%s"
               " submitted=%d attempted=%d generated_poses=%d"
               " elapsed_s=%.3f hdmi_wire_image=UNVERIFIED\n",
               status?"FAILED":"DONE",argv[2],submitted,total,unique_updates,
               now_seconds()-start);
        fflush(stdout);
    }
finish:
    if(frame)release_obj(frame);
    if(active){
        int32_t drc=((Disable)(*(void***)out)[7])(out);
        printf("stage=disable_output rc=0x%08x\n",(unsigned)drc);
        fflush(stdout);
        if(drc&&!status)status=29;
    }
    release_obj(out);
    release_obj(device);
    dlclose(lib);
    printf("stage=complete exit=%d capture_enabled=0"
           " input_hdmi_replugged=0 playback_only=1\n",status);
    fflush(stdout);
    return status;
}

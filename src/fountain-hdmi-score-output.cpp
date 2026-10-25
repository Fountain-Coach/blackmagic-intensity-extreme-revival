/*
 * Fountain Coach / DIE MASCHINE / Physical Video Output
 *
 * A Fountain-owned animated frame source feeding the original Intensity
 * Extreme over its existing installed DeckLinkAPI runtime. This *application*
 * is independent of Black Syphon; the native-kernel driver is still vendor 10.4.
 *
 * Capture is deliberately never started: original A124 is half-duplex.
 * Output only, max 180 frames, explicit supported 720p60 or 1080p25.
 * Pixel generator is the same source as Fountain's Syphon compositor.
 *
 * No PID/driver/firmware modifications, no vendor headers redistributed.
 * Actual Panasonic optical image remains separately unverified.
 */
#include "fountain-video-score.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <signal.h>
#include <time.h>

typedef void* (*Factory)(void);
typedef int32_t (*Query)(void*,CFUUIDBytes,void**);
typedef int32_t (*Next)(void*,void**);
typedef uint32_t (*Release)(void*);
typedef int32_t (*GetModelName)(void*,CFStringRef*);
typedef int32_t (*EnableOutput)(void*,uint32_t,uint32_t);
typedef int32_t (*DisableOutput)(void*);
typedef int32_t (*CreateFrame)(void*,int32_t,int32_t,int32_t,uint32_t,uint32_t,void**);
typedef int32_t (*GetBytes)(void*,void**);
typedef int32_t (*DisplaySync)(void*,void*);
typedef int32_t (*GetModes)(void*,void**);
typedef uint32_t (*GetModeCode)(void*);
static const unsigned char kOutputIID[16] = {
    0xcc,0x5c,0x8a,0x6e,0x3f,0x2f,0x4b,0x3a,
    0x87,0xea,0xfd,0x78,0xaf,0x30,0x05,0x64
};
static volatile sig_atomic_t gStop=0;
static void on_signal(int sig) { (void)sig;gStop=1; }
static void fc_release(void* obj) {
    if(obj)((Release)(*(void***)obj)[2])(obj);
}
static uint64_t milliseconds(void) {
    struct timespec now;
    if(clock_gettime(CLOCK_MONOTONIC,&now)!=0)return 0;
    return (uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u;
}
static int exact_original_machine(void) {
    char model[64]={0},release[64]={0};
    size_t ml=sizeof(model),rl=sizeof(release);
    return sysctlbyname("hw.model",model,&ml,NULL,0)==0 &&
           sysctlbyname("kern.osrelease",release,&rl,NULL,0)==0 &&
           strcmp(model,"Macmini5,3")==0 && strncmp(release,"16.",3)==0;
}
static int has_mode(void* output,uint32_t requested) {
    GetModes getModes=(GetModes)(*(void***)output)[4];
    void* modes=NULL;
    if(getModes(output,&modes)!=0||!modes)return 0;
    Next next=(Next)(*(void***)modes)[3];
    int available=0;
    for(int i=0;i<64;i++){
        void* mode=NULL;
        if(next(modes,&mode)!=0||!mode)break;
        uint32_t code=((GetModeCode)(*(void***)mode)[4])(mode);
        if(code==requested)available++;
        fc_release(mode);
    }
    fc_release(modes);
    return available==1;
}
static int do_run(const char* format,int frameCount) {
    if(!exact_original_machine()){
        puts("stage=hardware_guard result=wrong_machine exit=20");return 20;
    }
    const int is720=!strcmp(format,"720p60");
    const int width=is720?1280:1920,height=is720?720:1080;
    const uint32_t mode=is720?0x68703630U:0x48703235U;
    const size_t frameBytes=(size_t)width*height*2u;
    void* lib=NULL,*iterator=NULL,*device=NULL,*output=NULL,*frame=NULL;
    int enabled=0,exitCode=0,submitted=0;
    uint32_t firstCrc=0,lastCrc=0;
    uint64_t firstAt=0,lastAt=0;
    lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
               RTLD_NOW|RTLD_LOCAL);
    if(!lib){puts("stage=decklink result=framework_unavailable");return 21;}
    Factory factory=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    iterator=factory?factory():NULL;
    if(!iterator){exitCode=22;goto cleanup;}
    if(((Next)(*(void***)iterator)[3])(iterator,&device)!=0||!device) {
        exitCode=23;goto cleanup;
    }
    {
        CFStringRef cfModel=NULL;
        GetModelName get=(GetModelName)(*(void***)device)[3];
        char name[128]={0};
        if(get(device,&cfModel)!=0||!cfModel) {exitCode=24;goto cleanup;}
        int converted=CFStringGetCString(cfModel,name,sizeof(name),kCFStringEncodingUTF8);
        CFRelease(cfModel);
        if(!converted||strcmp(name,"Intensity Extreme")){
            printf("stage=device result=wrong_model name=%s\n",name);
            exitCode=24;goto cleanup;
        }
        printf("stage=device result=Intensity_Extreme api=original_10_4"
               " source=Fountain_Owned_Score output_only=1\n");
        fflush(stdout);
    }
    {
        CFUUIDBytes id;memcpy(&id,kOutputIID,sizeof(id));
        if(((Query)(*(void***)device)[0])(device,id,&output)!=0||!output){
            exitCode=25;goto cleanup;
        }
    }
    if(!has_mode(output,mode)){
        puts("stage=output result=unsupported_hardware_mode");
        exitCode=26;goto cleanup;
    }
    printf("stage=output mode=%s width=%d height=%d requested_frames=%d"
           " buffers=UYVY capture_started=0\n",format,width,height,frameCount);
    fflush(stdout);
    {
        int32_t rc=((EnableOutput)(*(void***)output)[6])(output,mode,0);
        printf("stage=enable_output status=0x%08x\n",(unsigned)rc);
        fflush(stdout);
        if(rc!=0){exitCode=27;goto cleanup;}
    }
    enabled=1;
    {
        int32_t rc=((CreateFrame)(*(void***)output)[9])(
            output,width,height,width*2,0x32767579U,0,&frame);
        if(rc!=0||!frame){
            printf("stage=create_frame status=0x%08x\n",(unsigned)rc);
            exitCode=28;goto cleanup;
        }
    }
    {
        void* data=NULL;
        int32_t rc=((GetBytes)(*(void***)frame)[8])(frame,&data);
        if(rc!=0||!data){
            printf("stage=get_buffer status=0x%08x\n",(unsigned)rc);
            exitCode=29;goto cleanup;
        }
        uint8_t* p=(uint8_t*)data;
        for(int i=0;i<frameCount&&!gStop;i++) {
            if(fc_video_score_uyvy(p,frameBytes,width,height,(uint32_t)i)!=0){
                exitCode=30;break;
            }
            uint32_t crc=fc_video_score_crc(p,frameBytes);
            if(i==0)firstCrc=crc;
            lastCrc=crc;
            uint64_t start=milliseconds();
            int32_t rc=((DisplaySync)(*(void***)output)[11])(output,frame);
            uint64_t end=milliseconds();
            if(rc!=0){
                printf("stage=frame_submit result=failed"
                       " index=%d status=0x%08x\n",i,(unsigned)rc);
                exitCode=31;break;
            }
            if(i==0)firstAt=start;
            lastAt=end;
            submitted++;
            if((i%20)==0 || i==frameCount-1){
                printf("stage=video_frame_submit index=%d sent=%d crc=0x%08x"
                       " display_sync_ms=%llu\n",i,submitted,(unsigned)crc,
                       (unsigned long long)(end-start));
                fflush(stdout);
            }
        }
    }
cleanup:
    /* Preserve vendor frame lifecycle convention; crash-prone input frame
       release was a separate historical driver callback path. */
    if(frame)fc_release(frame);
    if(enabled&&output){
        int32_t rc=((DisableOutput)(*(void***)output)[7])(output);
        printf("stage=disable_output status=0x%08x\n",(unsigned)rc);
        if(rc && !exitCode)exitCode=32;
    }
    fc_release(output);
    fc_release(device);
    fc_release(iterator);
    if(lib)dlclose(lib);
    printf("stage=complete exit=%d submitted=%d requested=%d"
           " first_crc=0x%08x last_crc=0x%08x"
           " render_changed=%d submission_window_ms=%llu"
           " image_optically_verified=0 capture_stream_started=0"
           " original_driver_replaced=0\n",
           exitCode,submitted,frameCount,(unsigned)firstCrc,(unsigned)lastCrc,
           submitted>1 && firstCrc!=lastCrc,
           (unsigned long long)((lastAt>=firstAt)?lastAt-firstAt:0));
    return exitCode;
}
int main(int argc,char**argv){
    if(argc!=4 || strcmp(argv[1],"--output")) {
        puts("usage: fc-video-hdmi-output --output 720p60|1080p25 <frames_1_to_180>");
        return 64;
    }
    if(strcmp(argv[2],"720p60")&&strcmp(argv[2],"1080p25"))return 64;
    char* end=NULL;
    long frames=strtol(argv[3],&end,10);
    if(!end||*end||frames<1||frames>180)return 64;
    signal(SIGTERM,on_signal);
    signal(SIGINT,on_signal);
    return do_run(argv[2],(int)frames);
}

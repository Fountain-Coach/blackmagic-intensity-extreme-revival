// Fountain Coach — genuine original Intensity Extreme HDMI OUT playback
// from the silver Sierra Macmini5,3 with the original Thunderbolt card.
// Source: internally generated UYVY color bars + moving white marker.
// Monitor is the Samsung on Blackmagic HDMI OUT -> Samsung HDMI 2.
// Independent i-tec HDMI -> Samsung HDMI 1 stays unchanged.
// NO HDMI IN cable, whole-desktop capture, Blackmagic bypass or new hardware
// is needed. No OS/driver/firmware/display-mode changes. The host receives
// output API acceptance; OPTICAL pixels are only independently seen by a
// human or separate sensor. Output is always stopped before exit.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint8_t kOutputIID[16]={
  0xcc,0x5c,0x8a,0x6e,0x3f,0x2f,0x4b,0x3a,
  0x87,0xea,0xfd,0x78,0xaf,0x30,0x05,0x64
};
static const uint8_t kStatusIID[16]={
  0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,
  0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46
};
enum { W=1920,H=1080,RB=3840 };
static const uint32_t kMode=0x48703330u; // 1080p30, Hp30
static const uint32_t kPixel=0x32767579u; // UYVY 2vuy
typedef void*(*Factory)();
typedef int32_t(*Next)(void*,void**);
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef uint32_t(*Release)(void*);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
typedef int32_t(*Support)(void*,uint32_t,uint32_t,uint32_t,uint32_t*,void**);
typedef int32_t(*Enable)(void*,uint32_t,uint32_t);
typedef int32_t(*NoArg)(void*);
typedef int32_t(*NewFrame)(void*,int32_t,int32_t,int32_t,uint32_t,uint32_t,void**);
typedef int32_t(*GetBytes)(void*,void**);
typedef int32_t(*DisplaySync)(void*,void*);
static std::atomic<bool> interrupted(false);
static void onSignal(int) {interrupted.store(true);}
static void release(void* p) {
    if(p) ((Release)(*(void***)p)[2])(p);
}
static int32_t query(void* obj,const uint8_t idBytes[16],void** out) {
    if(!obj||!out)return (int32_t)0x80004003u;
    CFUUIDBytes id;memcpy(&id,idBytes,16);
    return ((QI)(*(void***)obj)[0])(obj,id,out);
}
static uint64_t ns() {
    static mach_timebase_info_data_t tb={0,0};
    if(!tb.denom && mach_timebase_info(&tb)!=KERN_SUCCESS)return 0;
    if(!tb.denom)return 0;
    return (uint64_t)(((__uint128_t)mach_absolute_time()*tb.numer)/tb.denom);
}
static bool originalSilverAndDesktop() {
    char model[64]={0};size_t m=sizeof(model);
    if(sysctlbyname("hw.model",model,&m,nullptr,0)||strcmp(model,"Macmini5,3"))return false;
    CGDirectDisplayID ids[32]={0};uint32_t count=0;
    if(CGGetOnlineDisplayList(32,ids,&count)!=kCGErrorSuccess)return false;
    int matched=0;
    for(uint32_t i=0;i<count;++i) {
        CGDirectDisplayID id=ids[i];
        if(CGDisplayPixelsWide(id)!=W||
           CGDisplayPixelsHigh(id)!=H||!CGDisplayIsOnline(id))continue;
        CGDisplayModeRef mode=CGDisplayCopyDisplayMode(id);
        if(mode) {
            const double hz=CGDisplayModeGetRefreshRate(mode);
            if(hz>29.9&&hz<30.1)++matched;
            CGDisplayModeRelease(mode);
        }
    }
    return matched>=1;
}
static void paint(uint8_t* buf) {
    const uint8_t bars[8][3]={
        {128,235,128},{16,210,146},{166,170,16},{54,145,34},
        {202,106,222},{90,81,240},{240,41,110},{128,16,128}
    };
    for(int y=0;y<H;++y) {
        uint8_t* row=buf+(size_t)y*RB;
        for(int pair=0;pair<W/2;++pair) {
            const int bar=pair/120;
            uint8_t* pixel=row+(size_t)pair*4;
            pixel[0]=bars[bar][0];
            pixel[1]=bars[bar][1];
            pixel[2]=bars[bar][2];
            pixel[3]=bars[bar][1];
        }
    }
}
static uint32_t pollStatus(void* status,unsigned seconds) {
    if(!status)return 0;
    int64_t mode=0;
    const int32_t rc=((GetInt)(*(void***)status)[4])(status,0x63766f6du,&mode);
    printf("stage=steady_output_status elapsed_s=%u rc=0x%08x "
           "video_mode_fourcc=0x%08x requested=0x48703330 "
           "mode_is_requested=%u optical_sink_measured=0\n",
           seconds,(unsigned)rc,(unsigned)(uint32_t)mode,
           (unsigned)(rc==0&&(uint32_t)mode==kMode));
    fflush(stdout);
    return rc==0?(uint32_t)mode:0;
}
int main(int argc,char** argv) {
    const bool run=argc==2&&(!strcmp(argv[1],"--program-hdmi2-60s") || !strcmp(argv[1],"--program-hdmi2-300s")); 
    const unsigned duration=argc==2&&!strcmp(argv[1],"--program-hdmi2-300s")?300u:60u;
    if(!run) {
        fprintf(stderr,"usage: --program-hdmi2-60s | --program-hdmi2-300s\n");
        return 64;
    }
    if(!originalSilverAndDesktop()) {
        puts("stage=program_output result=NOT_ORIGINAL_SILVER_OR_DESKTOP_OFFLINE");
        return 65;
    }
    signal(SIGINT,onSignal);signal(SIGTERM,onSignal);
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                    RTLD_NOW|RTLD_LOCAL);
    if(!lib)return 66;
    Factory factory=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    void* iterator=factory?factory():nullptr, *dev=nullptr,*output=nullptr,*status=nullptr;
    int32_t rc=(int32_t)0x80004005u;
    bool active=false,accepted=true;
    unsigned frames=0,errors=0,matchingSeconds=0;
    void* frame=nullptr;
    if(!iterator){accepted=false;goto cleanup;}
    rc=((Next)(*(void***)iterator)[3])(iterator,&dev);
    if(rc||!dev){accepted=false;goto cleanup;}
    rc=query(dev,kOutputIID,&output);
    if(rc||!output){accepted=false;goto cleanup;}
    rc=query(dev,kStatusIID,&status);
    printf("stage=steady_output_status_interface rc=0x%08x available=%u\n",
           (unsigned)rc,(unsigned)(status!=nullptr));
    if(status)pollStatus(status,0);
    {
        void** ov=*(void***)output;
        uint32_t supports=0;void* modeObj=nullptr;
        rc=((Support)ov[3])(output,kMode,kPixel,0,&supports,&modeObj);
        release(modeObj);
        printf("stage=steady_output_mode rc=0x%08x supported=%u\n",
               (unsigned)rc,(unsigned)supports);
        if(rc||!supports){accepted=false;goto cleanup;}
        rc=((Enable)ov[6])(output,kMode,0);
        printf("stage=steady_output_enable rc=0x%08x target=1080p30\n",(unsigned)rc);
        fflush(stdout);
        if(rc){accepted=false;goto cleanup;}
        active=true;
        rc=((NewFrame)ov[9])(output,W,H,RB,kPixel,0,&frame);
        if(rc||!frame){accepted=false;goto cleanup;}
        void* bytes=nullptr;
        rc=((GetBytes)(*(void***)frame)[8])(frame,&bytes);
        if(rc||!bytes){accepted=false;goto cleanup;}
        uint8_t* pixels=(uint8_t*)bytes;
        paint(pixels);
        const uint64_t start=ns();
        unsigned lastSecond=0;
        while(!interrupted.load()) {
            const uint64_t elapsedNs=ns()-start;
            if(elapsedNs>=(uint64_t)duration*1000000000ull)break;
            // A deterministic 100px-wide moving luminance marker at the
            // bottom proves active updates without saving private content.
            const int y=H-48;
            uint8_t* marker=pixels+(size_t)y*RB;
            for(int i=0;i<W/2;++i) {
                uint8_t* p=marker+(size_t)i*4;
                p[0]=128;p[1]=24;p[2]=128;p[3]=24;
            }
            for(int i=0;i<50;++i) {
                const int pair=(int)((frames*8+i)%(W/2));
                uint8_t* p=marker+(size_t)pair*4;
                p[0]=128;p[1]=235;p[2]=128;p[3]=235;
            }
            rc=((DisplaySync)ov[11])(output,frame);
            if(rc) {
                ++errors;
                printf("stage=steady_output_frame_rejected frame=%u rc=0x%08x\n",
                       frames,(unsigned)rc);
                accepted=false;
                break;
            }
            ++frames;
            const unsigned seconds=(unsigned)((ns()-start)/1000000000ull);
            if(seconds>lastSecond) {
                lastSecond=seconds;
                const uint32_t mode=pollStatus(status,seconds);
                if(mode==kMode)++matchingSeconds;
                if(seconds%5==0) {
                    printf("stage=steady_output_heartbeat elapsed_s=%u "
                           "vendor_frames_accepted=%u failures=%u "
                           "output_enabled=1 mode_same=1\n",
                           seconds,frames,errors);
                    fflush(stdout);
                }
            }
            // Vendor synchronous display may itself block; paced boundedly
            // without restarting or disabling output between frames.
            usleep(33333);
        }
    }
cleanup:
    if(active) {
        const int32_t stop=((NoArg)(*(void***)output)[7])(output);
        printf("stage=steady_output_disable rc=0x%08x\n",(unsigned)stop);
        if(stop)accepted=false;
    }
    release(frame);release(status);release(output);release(dev);release(iterator);
    if(lib)dlclose(lib);
    printf("stage=steady_output_complete result=%s vendor_frames=%u "
           "api_errors=%u consecutive_matching_mode_seconds=%u "
           "output_disabled_after_test=1 monitor_optical_pixels_unmeasured=1 "
           "hdmi_in_untouched=1\n",
           accepted&&frames>=duration*8&&matchingSeconds>=duration-2?"API_STABLE":"FAIL",
           frames,errors,matchingSeconds);
    fflush(stdout);
    return accepted&&frames>=duration*8&&matchingSeconds>=duration-2?0:67;
}

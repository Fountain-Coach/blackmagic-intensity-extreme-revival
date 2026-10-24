// Source-owned mode-armed, no-stream capture-status probe.
// Fixed physical rig: silver Mac HDMI OUT -> original Intensity Extreme HDMI IN.
// Device accessed through Sierra's existing Desktop Video framework, no KEXT update.
// IMPORTANT: Enables the matching input briefly, DOES NOT call StartStreams or install
// any callback. No frame is ever classified as captured by this diagnostic.
#include <CoreFoundation/CoreFoundation.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <unistd.h>

typedef void* (*Factory)();
typedef int32_t (*Query)(void*, CFUUIDBytes, void**);
typedef int32_t (*Next)(void*, void**);
typedef int32_t (*Enable)(void*, uint32_t, uint32_t, uint32_t);
typedef int32_t (*NoArg)(void*);
typedef int32_t (*GetFlag)(void*, uint32_t, bool*);
typedef int32_t (*GetInt)(void*, uint32_t, int64_t*);
typedef uint32_t (*Release)(void*);

static const unsigned char kInputIID[16] = {
    0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,
    0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95
};
static const unsigned char kStatusIID[16] = {
    0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,
    0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46
};
static void queryStatus(void* status, int step) {
    if (!status) {
        std::printf("stage=mode_armed_status step=%d status=unavailable\n", step);
        return;
    }
    void** vt=*(void***)status;
    bool locked=false;
    int64_t detected=0, current=0;
    // These identifiers are supplied by Blackmagic's public API.
    const int32_t lrc=((GetFlag)vt[3])(status,0x7669736c,&locked); // 'visl'
    const int32_t drc=((GetInt)vt[4])(status,0x6476696d,&detected); // 'dvim'
    // Never interpret a failed GetFlag as "unlocked".
    std::printf("stage=mode_armed_status step=%d lock_rc=0x%08x lock=%s "
                "detected_rc=0x%08x detected_mode=0x%llx "
                "real_video_frame_accepted=0\n",
                step,(unsigned)lrc,lrc==0?(locked?"yes":"no"):"unknown",
                (unsigned)drc,(unsigned long long)detected);
    std::fflush(stdout);
    (void)current;
}
int main(int argc,char**argv) {
    if (argc!=2 || std::strcmp(argv[1],"--armed-1080p30-no-stream")!=0) {
        std::fprintf(stderr,"usage: %s --armed-1080p30-no-stream\n",argv[0]);
        return 64;
    }
    std::puts("stage=physical_topology silver_hdmi_out_to_intensity_hdmi_in=1 "
              "sierra_original_driver=1");
    std::puts("stage=safety callback_installed=0 start_streams=0 "
              "kext_changes=0 firmware_changes=0 guest_changes=0");
    std::fflush(stdout);
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                     RTLD_NOW|RTLD_LOCAL);
    if(!lib){ std::puts("stage=framework result=unavailable");return 20; }
    Factory factory=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    if(!factory){dlclose(lib);return 21;}
    void* iterator=factory();
    if(!iterator){dlclose(lib);return 22;}
    void** itV=*(void***)iterator;
    void* device=nullptr;
    int32_t rc=((Next)itV[3])(iterator,&device);
    ((Release)itV[2])(iterator);
    if(rc||!device){std::printf("stage=device rc=0x%08x\n",(unsigned)rc);dlclose(lib);return 23;}
    void** dv=*(void***)device;
    CFUUIDBytes uuid;
    std::memcpy(&uuid,kInputIID,16);
    void* input=nullptr;
    rc=((Query)dv[0])(device,uuid,&input);
    std::printf("stage=input rc=0x%08x present=%d\n",(unsigned)rc,input!=nullptr);
    if(rc||!input){((Release)dv[2])(device);dlclose(lib);return 24;}
    std::memcpy(&uuid,kStatusIID,16);
    void* status=nullptr;
    const int32_t src=((Query)dv[0])(device,uuid,&status);
    std::printf("stage=status rc=0x%08x present=%d\n",(unsigned)src,status!=nullptr);
    void** iv=*(void***)input;
    // Exact current macOS HDMI output: 1920x1080 30.00 progressive.
    // Enable 8-bit YUV for THIS ORIGINAL INTENSITY MODEL.
    rc=((Enable)iv[6])(input,0x48703330u,0x32767579u,0);
    std::printf("stage=enable_video_input rc=0x%08x requested=1080p30 "
                "pixel_format=2vuy\n",(unsigned)rc);
    std::fflush(stdout);
    int ret=0;
    if(rc==0) {
        for(int i=0;i<4;++i) {
            queryStatus(status,i);
            if(i<3) usleep(250000);
        }
        int32_t rdis=((NoArg)iv[7])(input);
        std::printf("stage=disable_video_input rc=0x%08x\n",(unsigned)rdis);
        if(rdis)ret=28;
    } else ret=26;
    if(status) {
        void** sv=*(void***)status;
        ((Release)sv[2])(status);
    }
    ((Release)iv[2])(input);
    ((Release)dv[2])(device);
    dlclose(lib);
    std::printf("stage=complete exit=%d stream_started=0 "
                "physical_frame_captured=0\n",ret);
    return ret;
}

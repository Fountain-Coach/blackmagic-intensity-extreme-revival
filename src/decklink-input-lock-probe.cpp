// Fountain Coach: source-owned, no-stream input-lock diagnostic for the
// original Blackmagic Intensity Extreme on Sierra 10.12.6.
// Commands:
//   --preflight-1080p25  : CoreGraphics only; never opens the vendor library.
//   --preflight-1080p30  : CoreGraphics only; never opens the vendor library.
//   --preflight-720p60   : CoreGraphics only; never opens the vendor library.
//   --probe-active-1080p25 : vendor input mode enable/status/disable, no stream.
//   --probe-active-1080p30 : vendor input mode enable/status/disable, no stream.
//   --probe-active-720p60  : vendor input mode enable/status/disable, no stream.
// The probe STRICTLY checks the actual physical BMD display's selected source
// rate before it arms input. Never changes source display mode or connector.
// No IDeckLinkInput SetCallback/StartStreams, frame memory, PCI MMIO, or
// firmware/driver operations. Old 10.4 frame-release crash stays contained.
// Copyright (c) 2026 Fountain Coach contributors. MIT.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

typedef void* (*Factory)(void);
typedef int32_t (*Query)(void*,CFUUIDBytes,void**);
typedef int32_t (*Next)(void*,void**);
typedef uint32_t (*Release)(void*);
typedef int32_t (*Enable)(void*,uint32_t,uint32_t,uint32_t);
typedef int32_t (*NoArg)(void*);
typedef int32_t (*GetFlag)(void*,uint32_t,bool*);
typedef int32_t (*GetInt)(void*,uint32_t,int64_t*);

static const uint8_t kInputIID[16]={
    0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,
    0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95};
static const uint8_t kStatusIID[16]={
    0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,
    0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static const uint8_t kConfigIID[16]={
    0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,
    0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb};

static void release_object(void* v) {
    if (v) ((Release)(*(void***)v)[2])(v);
}
static int32_t query(void* obj, const uint8_t raw[16], void** out) {
    *out=nullptr;
    CFUUIDBytes id;memcpy(&id,raw,16);
    return ((Query)(*(void***)obj)[0])(obj,id,out);
}

static int source_mode_preflight(uint32_t expected_width,
                                 uint32_t expected_height,
                                 uint32_t expected_rate,
                                 bool must_be_active) {
    CGDirectDisplayID ids[16];uint32_t n=0;
    const CGError gr=CGGetOnlineDisplayList(16,ids,&n);
    if (gr!=kCGErrorSuccess || n!=1) {
        printf("stage=source result=invalid_display_count count=%u cg_error=%d\n",
               n,(int)gr);
        return 21;
    }
    const CGDirectDisplayID d=ids[0];
    const uint32_t vendor=CGDisplayVendorNumber(d);
    const uint32_t product=CGDisplayModelNumber(d);
    if(vendor!=8961 || product!=0) {
        printf("stage=source result=not_direct_bmd_edid vendor=%u product=%u\n",
               vendor,product);return 22;
    }
    CGDisplayModeRef active=CGDisplayCopyDisplayMode(d);
    if(!active) { puts("stage=source result=no_active_mode");return 23; }
    size_t w=CGDisplayModeGetWidth(active),h=CGDisplayModeGetHeight(active);
    double refresh=CGDisplayModeGetRefreshRate(active);
    CGDisplayModeRelease(active);
    CFArrayRef modes=CGDisplayCopyAllDisplayModes(d,nullptr);
    bool available=false;
    if(modes){
        CFIndex count=CFArrayGetCount(modes);
        for(CFIndex i=0;i<count;i++){
            CGDisplayModeRef m=(CGDisplayModeRef)CFArrayGetValueAtIndex(modes,i);
            double r=CGDisplayModeGetRefreshRate(m);
            if(CGDisplayModeGetWidth(m)==expected_width && CGDisplayModeGetHeight(m)==expected_height
               && r>expected_rate-0.01 && r<expected_rate+0.01) available=true;
        }
        CFRelease(modes);
    }
    bool active_match=(w==expected_width && h==expected_height &&
                       refresh>expected_rate-0.01 &&
                       refresh<expected_rate+0.01);
    printf("stage=source result=BMD_HDMI active_width=%zu active_height=%zu"
           " selected_rate_hz=%.6f required_width=%u required_height=%u required_rate_hz=%u"
           " mode_advertised=%u active_mode_match=%u"
           " cg_display_only=1 mode_changes=0\n",
           w,h,refresh,expected_width,expected_height,expected_rate,available?1u:0u,
           active_match?1u:0u);
    if(!available) return 24;
    if(must_be_active && !active_match) {
        puts("stage=mode_guard result=BLOCKED_wrong_current_rate vendor_calls=0");
        return 25;
    }
    return 0;
}

int main(int argc,char** argv) {
    // Fail closed outside the exact 2011 Sierra witness machine.
    char model[64]={0}, osrelease[64]={0};
    size_t ml=sizeof(model), ol=sizeof(osrelease);
    if(sysctlbyname("hw.model",model,&ml,nullptr,0) ||
       sysctlbyname("kern.osrelease",osrelease,&ol,nullptr,0) ||
       strcmp(model,"Macmini5,3") || strncmp(osrelease,"16.",3)) {
        puts("stage=host result=wrong_silver_sierra_fixture vendor_calls=0");
        return 20;
    }
    if(argc!=2) {
        puts("usage: lock-probe --preflight-1080p25|--preflight-1080p30"
             "|--preflight-720p60|--probe-active-1080p25|--probe-active-1080p30|--probe-active-720p60");
        return 64;
    }
    bool preflight=false,probe=false;
    uint32_t rate=0,mode=0,width=1920,height=1080;
    if(!strcmp(argv[1],"--preflight-1080p25")){
        preflight=true;rate=25;
    }else if(!strcmp(argv[1],"--preflight-1080p30")){
        preflight=true;rate=30;
    }else if(!strcmp(argv[1],"--preflight-720p60")){
        preflight=true;rate=60;width=1280;height=720;
    }else if(!strcmp(argv[1],"--probe-active-1080p25")){
        probe=true;rate=25;mode=0x48703235u;
    }else if(!strcmp(argv[1],"--probe-active-1080p30")){
        probe=true;rate=30;mode=0x48703330u;
    }else if(!strcmp(argv[1],"--probe-active-720p60")){
        probe=true;rate=60;mode=0x68703630u;width=1280;height=720;
    }else return 64;

    const int gate=source_mode_preflight(width,height,rate,probe);
    if(gate) return gate;
    if(preflight){
        puts("stage=preflight result=pass display_modified=0"
             " receiver_lock=not_tested no_vendor_calls=1");
        return 0;
    }
    // From here on: original installed DeckLinkAPI 10.4 only.
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                     RTLD_NOW|RTLD_LOCAL);
    if(!lib){puts("stage=decklink result=framework_unavailable");return 40;}
    Factory factory=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    void* iter=factory?factory():nullptr;
    if(!iter){puts("stage=decklink result=no_iterator");dlclose(lib);return 41;}
    void* device=nullptr;
    int32_t rc=((Next)(*(void***)iter)[3])(iter,&device);
    release_object(iter);
    if(rc||!device){
        printf("stage=decklink result=no_device rc=0x%08x\n",(unsigned)rc);
        dlclose(lib);return 42;
    }
    void* input=nullptr;void* status=nullptr;void* config=nullptr;
    int result=0;bool enabled=false;
    rc=query(device,kConfigIID,&config);
    if(rc||!config){result=43;goto cleanup;}
    {
        int64_t connector=0;
        rc=((GetInt)(*(void***)config)[6])(config,0x7669636eU,&connector);
        printf("stage=input_connector rc=0x%08x value=0x%llx expected=0x2\n",
               (unsigned)rc,(unsigned long long)connector);
        if(rc||(uint32_t)connector!=2u){result=44;goto cleanup;}
    }
    rc=query(device,kInputIID,&input);
    if(rc||!input){result=45;goto cleanup;}
    rc=query(device,kStatusIID,&status);
    if(rc||!status){result=46;goto cleanup;}
    rc=((Enable)(*(void***)input)[6])(input,mode,0x32767579u,0);
    printf("stage=enable_input mode=%s pixel_buffer=2vuy rc=0x%08x\n",
           rate==60?"720p60":(rate==30?"1080p30":"1080p25"),(unsigned)rc);
    fflush(stdout);
    if(rc){result=47;goto cleanup;}
    enabled=true;
    {
        unsigned locked_samples=0,mode_samples=0;
        for(unsigned i=0;i<8;i++){
            bool locked=false;int64_t detected=0;
            int32_t lock_rc=((GetFlag)(*(void***)status)[3])(
                status,0x7669736cu,&locked);
            int32_t mode_rc=((GetInt)(*(void***)status)[4])(
                status,0x6476696du,&detected);
            if(lock_rc==0&&locked) locked_samples++;
            if(mode_rc==0 && (uint32_t)detected==mode) mode_samples++;
            printf("stage=receiver sample=%u lock_rc=0x%08x"
                   " locked=%s detected_rc=0x%08x detected_mode=0x%llx\n",
                   i,(unsigned)lock_rc,
                   lock_rc==0?(locked?"YES":"NO"):"UNKNOWN",
                   (unsigned)mode_rc,(unsigned long long)detected);
            fflush(stdout);
            if(i<7) usleep(250000);
        }
        printf("stage=receiver result=%s locked_samples=%u"
               " correct_detected_mode_samples=%u"
               " real_frame_acceptance=NOT_TESTED\n",
               (locked_samples>=2 && mode_samples>=2)?"LOCK_CANDIDATE":"NO_LOCK",
               locked_samples,mode_samples);
        if(locked_samples<2 || mode_samples<2) result=32;
    }
cleanup:
    if(enabled){
        int32_t drc=((NoArg)(*(void***)input)[7])(input);
        printf("stage=disable_input rc=0x%08x\n",(unsigned)drc);
        if(drc) result=48;
    }
    release_object(status);
    release_object(input);
    release_object(config);
    release_object(device);
    dlclose(lib);
    printf("stage=complete exit=%d callback_installed=0"
           " stream_started=0 captured_frames=0"
           " display_mode_changes=0 driver_firmware_changes=0\n",result);
    return result;
}

// Original 2011 silver Sierra / Intensity Extreme Desktop Video 10.4.
// Reversible, bounded experiment ONLY. NEVER calls WriteToPreferences.
// Reasserts the actual known 1080p30 source as device default output and
// Last Played Frame on idle, then restores original Hi50/black values.
//
// This isolates the HDMI monitor blank/relock observed when the original
// Blackmagic card returns to idle after successful short 1080p30 playback.
// No OS driver, source, firmware, KEXT, PCI or disk writes.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
static const uint8_t kConfigIID[16]={
    0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,
    0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb
};
static const uint8_t kStatusIID[16]={
    0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,
    0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46
};
static const uint32_t kIdle=0x766f696fu; // 'voio'
static const uint32_t kDefault=0x64766f6du; // 'dvom'
static const uint32_t kBlack=0x626c6163u; // 'blac'
static const uint32_t kLast=0x6c616661u; // 'lafa'
static const uint32_t kHi50=0x48693530u; // 1080i50
static const uint32_t kHp30=0x48703330u; // 1080p30
typedef void*(*Factory)();
typedef int32_t(*Next)(void*,void**);
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef uint32_t(*Release)(void*);
typedef int32_t(*SetInt)(void*,uint32_t,int64_t);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
static void release(void* p){if(p)((Release)(*(void***)p)[2])(p);}
static int32_t query(void* x,const uint8_t idBytes[16],void** out){
    CFUUIDBytes u;memcpy(&u,idBytes,16);
    return ((QI)(*(void***)x)[0])(x,u,out);
}
static bool sourceMatches() {
    char model[64]={0};size_t n=sizeof(model);
    if(sysctlbyname("hw.model",model,&n,nullptr,0)||strcmp(model,"Macmini5,3"))
        return false;
    CGDirectDisplayID ids[32]={0};uint32_t nIds=0;
    if(CGGetOnlineDisplayList(32,ids,&nIds)!=kCGErrorSuccess)return false;
    unsigned matching=0;
    for(uint32_t i=0;i<nIds;++i){
        CGDirectDisplayID d=ids[i];
        if(CGDisplayVendorNumber(d)!=8961||CGDisplayPixelsWide(d)!=1920||
           CGDisplayPixelsHigh(d)!=1080)continue;
        CGDisplayModeRef mode=CGDisplayCopyDisplayMode(d);
        if(mode){
            const double hz=CGDisplayModeGetRefreshRate(mode);
            if(hz>29.9&&hz<30.1)++matching;
            CGDisplayModeRelease(mode);
        }
    }
    return matching==1;
}
static uint32_t statusMode(void* status){
    if(!status)return 0;
    int64_t v=0;
    int32_t rc=((GetInt)(*(void***)status)[4])(status,0x63766f6du,&v);
    printf("stage=idle_output_status rc=0x%08x mode_fourcc=0x%08x "
           "currently_progressive_30=%u\n",(unsigned)rc,
           (unsigned)(uint32_t)v,(unsigned)(rc==0&&(uint32_t)v==kHp30));
    fflush(stdout);
    return rc==0?(uint32_t)v:0;
}
int main(int argc,char**argv) {
    const bool blackOnly=argc==2 && !strcmp(argv[1],"--test-p30-black-8s");
    const bool lastFrame=argc==2 && !strcmp(argv[1],"--test-p30-idle-12s");
    if(!blackOnly && !lastFrame)return 64;
    const unsigned seconds=blackOnly?8u:12u;
    if(!sourceMatches()){
        puts("stage=idle_output_transient result=WRONG_SILVER_RIG");
        return 65;
    }
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                    RTLD_NOW|RTLD_LOCAL);
    if(!lib)return 66;
    Factory f=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    void* it=f?f():nullptr,*dev=nullptr,*cfg=nullptr,*status=nullptr;
    if(!it){dlclose(lib);return 67;}
    int32_t rc=((Next)(*(void***)it)[3])(it,&dev);
    release(it);
    if(rc||!dev){dlclose(lib);return 68;}
    rc=query(dev,kConfigIID,&cfg);
    if(rc||!cfg){release(dev);dlclose(lib);return 69;}
    query(dev,kStatusIID,&status);
    GetInt get=(GetInt)(*(void***)cfg)[6];
    SetInt set=(SetInt)(*(void***)cfg)[5];
    int64_t idle=-1,mode=-1;
    int32_t idleRc=get(cfg,kIdle,&idle);
    int32_t modeRc=get(cfg,kDefault,&mode);
    printf("stage=idle_output_original idle_rc=0x%08x idle=0x%08x "
           "default_rc=0x%08x default_mode=0x%08x\n",
           (unsigned)idleRc,(unsigned)(uint32_t)idle,
           (unsigned)modeRc,(unsigned)(uint32_t)mode);
    if(idleRc||modeRc||(uint32_t)idle!=kBlack||(uint32_t)mode!=kHi50) {
        puts("stage=idle_output_transient result=unexpected_original_settings");
        release(status);release(cfg);release(dev);dlclose(lib);return 70;
    }
    statusMode(status);
    bool changedMode=false, changedIdle=false, okay=true;
    rc=set(cfg,kDefault,(int64_t)kHp30);
    printf("stage=idle_output_set_mode rc=0x%08x temporary=1\n",(unsigned)rc);
    if(rc)okay=false;
    else changedMode=true;
    if(okay && lastFrame){
        rc=set(cfg,kIdle,(int64_t)kLast);
        printf("stage=idle_output_set_last_frame rc=0x%08x temporary=1\n",
               (unsigned)rc);
        if(rc)okay=false;
        else changedIdle=true;
    }
    if(okay)for(unsigned tick=0;tick<seconds;++tick) {
        int64_t nowIdle=0,nowMode=0;
        int32_t a=get(cfg,kIdle,&nowIdle),m=get(cfg,kDefault,&nowMode);
        printf("stage=idle_output_transient_state second=%u "
               "idle_get_rc=0x%08x idle_value=0x%08x "
               "mode_get_rc=0x%08x mode_value=0x%08x\n",
               tick,(unsigned)a,(unsigned)(uint32_t)nowIdle,
               (unsigned)m,(unsigned)(uint32_t)nowMode);
        if(a||m||(uint32_t)nowIdle!=(blackOnly?kBlack:kLast)||
           (uint32_t)nowMode!=kHp30)
            okay=false;
        statusMode(status);
        fflush(stdout);
        if(!okay)break;
        sleep(1);
    }
    // Restore exactly original known configuration, no persistence call.
    if(changedIdle){
        rc=set(cfg,kIdle,(int64_t)kBlack);
        printf("stage=idle_output_restore_idle rc=0x%08x\n",(unsigned)rc);
        if(rc)okay=false;
    }
    if(changedMode){
        rc=set(cfg,kDefault,(int64_t)kHi50);
        printf("stage=idle_output_restore_mode rc=0x%08x\n",(unsigned)rc);
        if(rc)okay=false;
    }
    {
        int64_t restoreIdle=0,restoreMode=0;
        const int32_t ri=get(cfg,kIdle,&restoreIdle);
        const int32_t rm=get(cfg,kDefault,&restoreMode);
        if(ri||rm||(uint32_t)restoreIdle!=kBlack||(uint32_t)restoreMode!=kHi50)
            okay=false;
        printf("stage=idle_output_restored idle=0x%08x default_mode=0x%08x "
               "matches_original=%u preferences_written=0\n",
               (unsigned)(uint32_t)restoreIdle,(unsigned)(uint32_t)restoreMode,
               (unsigned)(ri==0&&rm==0&&(uint32_t)restoreIdle==kBlack&&
                          (uint32_t)restoreMode==kHi50));
    }
    release(status);release(cfg);release(dev);dlclose(lib);
    printf("stage=idle_output_transient result=%s duration_s=%u "
           "idle_black_preserved=%u original_preserved=1 persisted=0 "
           "monitor_optical_witness=none\n",
           okay?"ACCEPTED":"FAILED",seconds,(unsigned)blackOnly);
    fflush(stdout);
    return okay?0:71;
}

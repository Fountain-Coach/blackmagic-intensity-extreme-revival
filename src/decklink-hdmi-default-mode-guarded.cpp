// Original 2011 silver Macmini5,3 / Sierra 10.12.6 / DeckLink 10.4
// Controlled device preference: ONLY DefaultVideoOutputMode ('dvom').
// 2011 silver HDMI OUT remains the only input signal source, fixed 1080p30.
// Original Blackmagic HDMI OUT goes to the local monitor; original KEXT kept.
//
// The original idle 'voio'='blac' MUST stay untouched (no private desktop
// freeze-frame disclosure). The previous idle mode 'dvom'='Hi50' made the
// display repeatedly reacquire sync when playback changed to 'Hp30'.
//
// Both the apply and rollback commands are fail-closed against any divergent
// hardware configuration. No sudo invocation; normal SDK privilege handling.
// The canonical original settings are backed up BEFORE invoking this tool.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef void*(*Factory)();
typedef int32_t(*Next)(void*,void**);
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
typedef int32_t(*SetInt)(void*,uint32_t,int64_t);
typedef int32_t(*WritePrefs)(void*);
typedef uint32_t(*Release)(void*);
static const uint8_t kIID[16]={0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,
                                 0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb};
static const uint32_t MODE_KEY=0x64766f6du; // dvom
static const uint32_t IDLE_KEY=0x766f696fu; // voio
static const uint32_t HI50=0x48693530u;
static const uint32_t HP30=0x48703330u;
static const uint32_t BLACK=0x626c6163u;
static void release(void* p){if(p)((Release)(*(void***)p)[2])(p);}
static bool rig() {
    char model[64]={0};size_t sz=sizeof(model);
    if(sysctlbyname("hw.model",model,&sz,nullptr,0)||strcmp(model,"Macmini5,3"))
        return false;
    CGDirectDisplayID arr[32]={0};uint32_t count=0;
    if(CGGetOnlineDisplayList(32,arr,&count)!=kCGErrorSuccess)return false;
    unsigned num=0;
    for(uint32_t i=0;i<count;++i){
        CGDirectDisplayID d=arr[i];
        if(CGDisplayVendorNumber(d)!=8961||
           CGDisplayPixelsWide(d)!=1920||CGDisplayPixelsHigh(d)!=1080||
           !CGDisplayIsOnline(d))continue;
        CGDisplayModeRef mode=CGDisplayCopyDisplayMode(d);
        if(mode){
            double hz=CGDisplayModeGetRefreshRate(mode);
            if(hz>29.9&&hz<30.1)++num;
            CGDisplayModeRelease(mode);
        }
    }
    return num==1;
}
int main(int argc,char**argv) {
    const bool apply=argc==2 && !strcmp(argv[1],"--set-default-hdmi-1080p30");
    const bool revert=argc==2 && !strcmp(argv[1],"--restore-default-hdmi-1080i50");
    if(!apply&&!revert)return 64;
    if(!rig()){
        puts("stage=hdmi_output_config result=wrong_final_silver_fixture");
        return 65;
    }
    const uint32_t oldMode=apply?HI50:HP30;
    const uint32_t newMode=apply?HP30:HI50;
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                    RTLD_NOW|RTLD_LOCAL);
    if(!lib)return 66;
    Factory f=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    void* it=f?f():nullptr,*dev=nullptr,*cfg=nullptr;
    if(!it){dlclose(lib);return 67;}
    int32_t rc=((Next)(*(void***)it)[3])(it,&dev);
    release(it);
    if(rc||!dev){dlclose(lib);return 68;}
    CFUUIDBytes iid;memcpy(&iid,kIID,16);
    rc=((QI)(*(void***)dev)[0])(dev,iid,&cfg);
    if(rc||!cfg){release(dev);dlclose(lib);return 69;}
    void** v=*(void***)cfg;
    GetInt get=(GetInt)v[6];
    SetInt set=(SetInt)v[5];
    WritePrefs save=(WritePrefs)v[11];
    int64_t previous=0,idle=0;
    const int32_t a=get(cfg,MODE_KEY,&previous);
    const int32_t b=get(cfg,IDLE_KEY,&idle);
    printf("stage=hdmi_output_config_before mode_rc=0x%08x "
           "default_fourcc=0x%08x idle_rc=0x%08x idle_fourcc=0x%08x\n",
           (unsigned)a,(unsigned)(uint32_t)previous,
           (unsigned)b,(unsigned)(uint32_t)idle);
    if(a||b||(uint32_t)idle!=BLACK||(uint32_t)previous!=oldMode){
        puts("stage=hdmi_output_config result=wrong_expected_start_state "
             "changed=0");
        release(cfg);release(dev);dlclose(lib);return 70;
    }
    rc=set(cfg,MODE_KEY,(int64_t)newMode);
    printf("stage=hdmi_output_config_set mode_only_rc=0x%08x "
           "target_fourcc=0x%08x idle_black_unchanged=1\n",
           (unsigned)rc,(unsigned)newMode);
    if(rc){release(cfg);release(dev);dlclose(lib);return 71;}
    int64_t changed=0,stillIdle=0;
    const int32_t x=get(cfg,MODE_KEY,&changed);
    const int32_t y=get(cfg,IDLE_KEY,&stillIdle);
    if(x||y||(uint32_t)changed!=newMode||(uint32_t)stillIdle!=BLACK){
        // Do NOT persist if a different field unexpectedly changed.
        int32_t undo=set(cfg,MODE_KEY,(int64_t)oldMode);
        printf("stage=hdmi_output_config result=unverified_changes "
               "rollback_rc=0x%08x preferences_written=0\n",(unsigned)undo);
        release(cfg);release(dev);dlclose(lib);return 72;
    }
    rc=save(cfg);
    printf("stage=hdmi_output_config_persist rc=0x%08x "
           "requires_vendor_preference_write=1\n",(unsigned)rc);
    if(rc){
        int32_t undo=set(cfg,MODE_KEY,(int64_t)oldMode);
        printf("stage=hdmi_output_config result=persist_unavailable "
               "rollback_rc=0x%08x no_sudo_escalation=1\n",(unsigned)undo);
        release(cfg);release(dev);dlclose(lib);return 73;
    }
    release(cfg);cfg=nullptr;
    rc=((QI)(*(void***)dev)[0])(dev,iid,&cfg);
    if(rc||!cfg){
        puts("stage=hdmi_output_config result=failed_independent_readback");
        release(dev);dlclose(lib);return 74;
    }
    int64_t observedMode=0,observedIdle=0;
    const int32_t mr=((GetInt)(*(void***)cfg)[6])(cfg,MODE_KEY,&observedMode);
    const int32_t ir=((GetInt)(*(void***)cfg)[6])(cfg,IDLE_KEY,&observedIdle);
    const bool ok=mr==0&&ir==0&&(uint32_t)observedMode==newMode&&
                  (uint32_t)observedIdle==BLACK;
    printf("stage=hdmi_output_config_after result=%s "
           "default_mode=0x%08x idle_mode=0x%08x "
           "original_mode=0x%08x source_mode_unchanged=1 "
           "driver_installed_unchanged=1 optical_monitor_measured=0\n",
           ok?"PERSISTED":"NOT_VERIFIED",
           (unsigned)(uint32_t)observedMode,(unsigned)(uint32_t)observedIdle,
           (unsigned)oldMode);
    release(cfg);release(dev);dlclose(lib);
    return ok?0:75;
}

// Diagnose actual original Blackmagic 10.4 input LOCK after monitor
// addition. Fixed fixture: silver Macmini5,3 HDMI OUT -> Intensity Extreme
// HDMI IN, original Intensity HDMI OUT -> reference monitor; original
// Thunderbolt 1 back to SAME Sierra Mac. No alternate HDMI sources.
//
// Bounded phases: inspect vendor input/output/busy state, optionally call
// DisableVideoOutput before EnableVideoInput on this simplex/half-duplex unit,
// hold the exact 1080p30 2vuy input enabled up to 15 seconds, inspect lock
// + current modes and finally DisableVideoInput. Never StartStreams,
// install callbacks, read pixels, change firmware, KEXT or Sierra display mode.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static const uint8_t kInputIID[16]={
    0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95};
static const uint8_t kOutputIID[16]={
    0xcc,0x5c,0x8a,0x6e,0x3f,0x2f,0x4b,0x3a,0x87,0xea,0xfd,0x78,0xaf,0x30,0x05,0x64};
static const uint8_t kStatusIID[16]={
    0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static const uint32_t Hp30=0x48703330, UYVY=0x32767579;
typedef void*(*Factory)();
typedef int32_t(*Next)(void*,void**);
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef int32_t(*EnableInput)(void*,uint32_t,uint32_t,uint32_t);
typedef int32_t(*NoArg)(void*);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
typedef int32_t(*GetFlag)(void*,uint32_t,bool*);
typedef uint32_t(*Release)(void*);
static void releaseObj(void* o){if(o)((Release)(*(void***)o)[2])(o);}
static int32_t query(void* object,const uint8_t iidRaw[16],void** out){
    if(!object)return (int32_t)0x80004003;
    CFUUIDBytes iid;memcpy(&iid,iidRaw,16);
    return ((QI)(*(void***)object)[0])(object,iid,out);
}
static bool sourceMatches(){
    char model[64]={0};size_t n=sizeof(model);
    if(sysctlbyname("hw.model",model,&n,nullptr,0)||strcmp(model,"Macmini5,3"))return false;
    CGDirectDisplayID arr[32]={0};uint32_t count=0;
    if(CGGetOnlineDisplayList(32,arr,&count)!=kCGErrorSuccess)return false;
    unsigned hit=0;
    for(uint32_t i=0;i<count;++i){
        CGDirectDisplayID d=arr[i];
        if(CGDisplayVendorNumber(d)!=8961||CGDisplayPixelsWide(d)!=1920||
           CGDisplayPixelsHigh(d)!=1080||!CGDisplayIsActive(d))continue;
        CGDisplayModeRef mode=CGDisplayCopyDisplayMode(d);
        if(!mode)continue;
        const double rate=CGDisplayModeGetRefreshRate(mode);
        if(rate>29.9 && rate<30.1)++hit;
        CGDisplayModeRelease(mode);
    }
    return hit==1;
}
static void inspect(void* status,const char* phase,unsigned sample,
                    unsigned* stable,uint32_t* inputBusy){
    if(!status)return;
    void** vt=*(void***)status;
    bool locked=false;int64_t busy=0,in=0,out=0,pixel=0;
    const int32_t lrc=((GetFlag)vt[3])(status,0x7669736c,&locked);
    const int32_t brc=((GetInt)vt[4])(status,0x62757379,&busy); // busy
    const int32_t irc=((GetInt)vt[4])(status,0x6376696d,&in); // cvim
    const int32_t orc=((GetInt)vt[4])(status,0x63766f6d,&out); // cvom
    const int32_t prc=((GetInt)vt[4])(status,0x63766970,&pixel); // cvip
    if(inputBusy)*inputBusy=(brc==0)?(uint32_t)busy:0xffffffff;
    if(stable){
        if(lrc==0&&locked)++*stable;else *stable=0;
    }
    printf("stage=real_silver_input phase=%s sample=%u lock_rc=0x%08x lock=%s "
           "stable=%u busy_rc=0x%08x busy=0x%08x "
           "current_input_rc=0x%08x current_input=0x%08x "
           "current_pixel_rc=0x%08x current_pixel=0x%08x "
           "current_output_rc=0x%08x current_output=0x%08x "
           "hdmi_wire_measured=0\n",
           phase,sample,(unsigned)lrc,lrc==0?(locked?"YES":"NO"):"UNKNOWN",
           stable?*stable:0,(unsigned)brc,(unsigned)(uint32_t)busy,
           (unsigned)irc,(unsigned)(uint32_t)in,
           (unsigned)prc,(unsigned)(uint32_t)pixel,
           (unsigned)orc,(unsigned)(uint32_t)out);
    fflush(stdout);
}
int main(int argc,char**argv){
    const bool reset=argc==2&&!strcmp(argv[1],"--release-idle-output-then-probe-15s");
    const bool plain=argc==2&&!strcmp(argv[1],"--probe-only-15s");
    if(!plain&&!reset){
        puts("usage: input-lock-lifecycle --probe-only-15s | --release-idle-output-then-probe-15s");
        return 64;
    }
    if(!sourceMatches()){puts("stage=real_silver_input result=WRONG_FIXED_HDMI_RIG");return 20;}
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",RTLD_NOW|RTLD_LOCAL);
    if(!lib)return 21;
    Factory maker=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    void* iter=maker?maker():nullptr,*dev=nullptr,*input=nullptr,*output=nullptr,*status=nullptr;
    if(!iter){dlclose(lib);return 22;}
    int32_t rc=((Next)(*(void***)iter)[3])(iter,&dev);
    releaseObj(iter);
    if(rc||!dev){dlclose(lib);return 23;}
    rc=query(dev,kStatusIID,&status);
    if(rc||!status){releaseObj(dev);dlclose(lib);return 24;}
    inspect(status,"before_enable",0,nullptr,nullptr);
    if(reset){
        int32_t q=query(dev,kOutputIID,&output);
        if(q||!output) {
            printf("stage=output_release result=unavailable rc=0x%08x\n",(unsigned)q);
            releaseObj(status);releaseObj(dev);dlclose(lib);
            return 25;
        }
        rc=((NoArg)(*(void***)output)[7])(output);
        printf("stage=output_release disable_without_current_playback_rc=0x%08x "
               "output_was_not_started_by_test=1\n",(unsigned)rc);
        fflush(stdout);
        releaseObj(output);
        inspect(status,"after_output_disable",0,nullptr,nullptr);
    }
    rc=query(dev,kInputIID,&input);
    if(rc||!input){
        releaseObj(status);releaseObj(dev);dlclose(lib);return 26;
    }
    rc=((EnableInput)(*(void***)input)[6])(input,Hp30,UYVY,0);
    printf("stage=real_silver_input enable_input_rc=0x%08x mode=Hp30 pixel=2vuy "
           "stream_started=0 device_firmware_changes=0\n",(unsigned)rc);
    fflush(stdout);
    unsigned stable=0,positive=0,unknown=0;
    if(rc==0) {
        for(unsigned k=0;k<60;++k){
            // 15 seconds, longer than the previous 2.75s attempt.
            bool locked=false;
            const int32_t lrc=((GetFlag)(*(void***)status)[3])(
                                      status,0x7669736c,&locked);
            if(lrc==0 && locked)++positive;
            else if(lrc!=0)++unknown;
            inspect(status,"armed_input",k,&stable,nullptr);
            if(stable>=2)break;
            if(k<59)usleep(250000);
        }
    }
    if(rc==0){
        int32_t dis=((NoArg)(*(void***)input)[7])(input);
        printf("stage=real_silver_input disable_input_rc=0x%08x\n",(unsigned)dis);
        if(dis)rc=dis;
    }
    inspect(status,"after_disable",0,nullptr,nullptr);
    printf("stage=real_silver_input result=%s "
           "stable_lock=%u locked_samples=%u unknown_samples=%u "
           "streams_started=0 frames_captured=0 optical_monitor_witness=0\n",
           stable>=2?"LOCK_REACQUIRED":"NO_LOCK",stable,positive,unknown);
    fflush(stdout);
    releaseObj(input);releaseObj(status);releaseObj(dev);dlclose(lib);
    return rc==0?(stable>=2?0:32):33;
}

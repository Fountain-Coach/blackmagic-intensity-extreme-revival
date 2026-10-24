// Original Intensity Extreme HDMI IN: reversible user-space input-selection
// reinitialization. Fixed sole source: silver Macmini5,3 HDMI OUT -> original
// Intensity Extreme HDMI IN, Thunderbolt back to same Sierra Mac.
// Reference monitor may be attached to original Intensity HDMI OUT.
//
// Uses the installed ORIGINAL Desktop Video 10.4 IDeckLinkConfiguration only.
// Toggle existing HDMI input -> analog component -> HDMI, verified; no
// preference writes; capture never starts unless input mode can be enabled.
// NO video streams, frame callbacks, audio, screenshot pixels, KEXT/firmware,
// operating system, PCI, USB, monitor or cable modifications.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static const uint8_t CONFIG_IID[16]={
 0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,
 0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb};
static const uint8_t INPUT_IID[16]={
 0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,
 0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95};
static const uint8_t STATUS_IID[16]={
 0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,
 0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static const uint32_t VICN=0x7669636eu;
static const uint32_t HDMI=0x2u, COMPONENT=0x8u;
typedef void*(*Factory)();typedef int32_t(*Next)(void*,void**);
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef uint32_t(*Release)(void*);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
typedef int32_t(*SetInt)(void*,uint32_t,int64_t);
typedef int32_t(*EnableInput)(void*,uint32_t,uint32_t,uint32_t);
typedef int32_t(*NoArg)(void*);typedef int32_t(*GetFlag)(void*,uint32_t,bool*);
static void rel(void* p){if(p)((Release)(*(void***)p)[2])(p);}
static int32_t query(void* obj,const uint8_t raw[16],void** out){
 CFUUIDBytes u;memcpy(&u,raw,16);return ((QI)(*(void***)obj)[0])(obj,u,out);
}
static bool rig(){
 char model[64]={0};size_t sz=sizeof(model);
 if(sysctlbyname("hw.model",model,&sz,nullptr,0)||strcmp(model,"Macmini5,3"))return false;
 CGDirectDisplayID ids[16];uint32_t n=0;
 if(CGGetOnlineDisplayList(16,ids,&n)!=kCGErrorSuccess||n!=1)return false;
 CGDirectDisplayID d=ids[0];
 if(CGDisplayVendorNumber(d)!=8961||CGDisplayPixelsWide(d)!=1920||
    CGDisplayPixelsHigh(d)!=1080)return false;
 CGDisplayModeRef m=CGDisplayCopyDisplayMode(d);
 if(!m)return false;
 const double hz=CGDisplayModeGetRefreshRate(m);
 CGDisplayModeRelease(m);
 return hz>=29.9&&hz<=30.1;
}
int main(int argc,char**argv){
 if(argc!=2||strcmp(argv[1],"--reselect-input-hdmi-and-verify-lock"))return 64;
 if(!rig()){
     puts("stage=physical_input_reselect result=WRONG_ACTIVE_BMD_SILVER_FIXTURE");
     return 65;
 }
 void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                 RTLD_NOW|RTLD_LOCAL);
 if(!lib)return 66;
 Factory f=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
 void* it=f?f():nullptr,*dev=nullptr,*cfg=nullptr,*input=nullptr,*status=nullptr;
 if(!it){dlclose(lib);return 67;}
 int32_t rc=((Next)(*(void***)it)[3])(it,&dev);rel(it);
 if(rc||!dev){dlclose(lib);return 68;}
 rc=query(dev,CONFIG_IID,&cfg);
 if(rc||!cfg){rel(dev);dlclose(lib);return 69;}
 void** cv=*(void***)cfg;
 GetInt get=(GetInt)cv[6];
 SetInt set=(SetInt)cv[5];
 int64_t initial=0,observed=0,restored=0;
 const int32_t initialRC=get(cfg,VICN,&initial);
 printf("stage=physical_input_reselect original_connector_rc=0x%08x "
        "original_connector=0x%08x expected_hdmi=0x00000002\n",
        (unsigned)initialRC,(unsigned)(uint32_t)initial);
 if(initialRC||(uint32_t)initial!=HDMI){
     puts("stage=physical_input_reselect result=WRONG_ORIGINAL_INPUT");
     rel(cfg);rel(dev);dlclose(lib);return 70;
 }
 rc=set(cfg,VICN,(int64_t)COMPONENT);
 int32_t x=get(cfg,VICN,&observed);
 printf("stage=physical_input_reselect temporary_component_set_rc=0x%08x "
        "read_rc=0x%08x observed=0x%08x\n",
        (unsigned)rc,(unsigned)x,(unsigned)(uint32_t)observed);
 bool changed=rc==0&&x==0&&(uint32_t)observed==COMPONENT;
 if(changed)usleep(550000);
 // The original HDMI setting is ALWAYS restored before enabling any input.
 const int32_t back=set(cfg,VICN,(int64_t)HDMI);
 const int32_t verified=get(cfg,VICN,&restored);
 printf("stage=physical_input_reselect restore_hdmi_rc=0x%08x "
        "readback_rc=0x%08x restored_connector=0x%08x "
        "changed_away=%u preferences_written=0\n",
        (unsigned)back,(unsigned)verified,(unsigned)(uint32_t)restored,
        changed?1u:0u);
 bool safe=back==0&&verified==0&&(uint32_t)restored==HDMI;
 rel(cfg);cfg=nullptr;
 if(!safe){
     puts("stage=physical_input_reselect result=FAILED_TO_VERIFY_ORIGINAL_HDMI");
     rel(dev);dlclose(lib);return 71;
 }
 if(!changed){
     puts("stage=physical_input_reselect result=INPUT_SELECTION_UNSUPPORTED "
          "original_restored=1");
     rel(dev);dlclose(lib);return 72;
 }
 usleep(850000);
 rc=query(dev,INPUT_IID,&input);
 if(rc||!input){rel(dev);dlclose(lib);return 73;}
 rc=query(dev,STATUS_IID,&status);
 if(rc||!status){rel(input);rel(dev);dlclose(lib);return 74;}
 rc=((EnableInput)(*(void***)input)[6])(input,0x48703330u,0x32767579u,0);
 printf("stage=physical_input_reselect enable_p30_rc=0x%08x\n",(unsigned)rc);
 unsigned consecutive=0,yes=0;
 if(rc==0){
     for(unsigned k=0;k<16;++k){
         bool lock=false;
         const int32_t lr=((GetFlag)(*(void***)status)[3])(
                            status,0x7669736cu,&lock);
         if(lr==0&&lock){++yes;++consecutive;}else consecutive=0;
         printf("stage=physical_input_reselect sample=%u rc=0x%08x "
                "locked=%s consecutive=%u\n",
                k,(unsigned)lr,lr==0?(lock?"YES":"NO"):"UNKNOWN",consecutive);
         fflush(stdout);
         if(consecutive>=2)break;
         if(k<15)usleep(250000);
     }
     int32_t disable=((NoArg)(*(void***)input)[7])(input);
     printf("stage=physical_input_reselect disable_input_rc=0x%08x\n",
            (unsigned)disable);
     if(disable)rc=disable;
 }
 printf("stage=physical_input_reselect result=%s "
        "original_input_hdmi_restored=1 "
        "signal_lock_samples=%u stream_started=0 frames_captured=0 "
        "firmware_driver_display_and_cabling_unchanged=1\n",
        consecutive>=2?"LOCK_REACQUIRED":"STILL_NO_LOCK",yes);
 rel(status);rel(input);rel(dev);dlclose(lib);
 return rc==0?(consecutive>=2?0:32):75;
}

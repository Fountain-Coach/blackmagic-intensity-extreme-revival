// Source-owned fixed silver HDMI signal-lock mode differential.
// NEVER changes Sierra source mode or its physical HDMI source cable.
// Only changes the ORIGINAL Intensity Extreme's requested *input* format.
// This device does not support input format autodetection; inspect precise
// neighboring standardized HDMI rates in a bounded/no-stream diagnostic.
// Fixed topology: silver 2011 Macmini5,3 HDMI OUT -> original Intensity HDMI
// IN, original Intensity HDMI OUT -> monitor, Thunderbolt -> SAME Sierra Mac.
// No kernel, firmware, OS, driver, network, recording, callbacks, pixel reads.
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct Mode {uint32_t mode;const char* label;};
const Mode kModes[] = {
 {0x48703330,"1080p30.000"},
 {0x48703239,"1080p29.970"},
 {0x48703235,"1080p25.000"},
 {0x48693539,"1080i59.940"}
};
const uint32_t kUYVY=0x32767579;
const uint8_t kInputIID[16]={
 0xaf,0x22,0x76,0x2b,0xdf,0xac,0x48,0x46,0xaa,0x79,0xfa,0x88,0x83,0x56,0x09,0x95};
const uint8_t kStatusIID[16]={
 0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
typedef void*(*Factory)();typedef int32_t(*Next)(void*,void**);
typedef uint32_t(*Release)(void*);typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef int32_t(*Support)(void*,uint32_t,uint32_t,uint32_t,uint32_t*,void**);
typedef int32_t(*Enable)(void*,uint32_t,uint32_t,uint32_t);
typedef int32_t(*NoArg)(void*);typedef int32_t(*GetFlag)(void*,uint32_t,bool*);
static void rel(void* p){if(p)((Release)(*(void***)p)[2])(p);}
static int32_t query(void* obj,const uint8_t bytes[16],void**out){
 CFUUIDBytes id;memcpy(&id,bytes,16);
 return ((QI)(*(void***)obj)[0])(obj,id,out);
}
static bool exactSilverSource(){
 char model[64]={0};size_t n=sizeof(model);
 if(sysctlbyname("hw.model",model,&n,nullptr,0)||strcmp(model,"Macmini5,3"))return false;
 CGDirectDisplayID ids[32];uint32_t count=0;
 if(CGGetOnlineDisplayList(32,ids,&count)!=kCGErrorSuccess)return false;
 unsigned good=0;
 for(uint32_t i=0;i<count;++i){
    CGDirectDisplayID id=ids[i];
    if(CGDisplayVendorNumber(id)!=8961||CGDisplayPixelsWide(id)!=1920||
       CGDisplayPixelsHigh(id)!=1080||!CGDisplayIsActive(id))continue;
    CGDisplayModeRef mode=CGDisplayCopyDisplayMode(id);
    if(mode){const double hz=CGDisplayModeGetRefreshRate(mode);
        if(hz>29.9&&hz<30.1)good++;
        CGDisplayModeRelease(mode);
    }
 }
 return good==1;
}
int main(int argc,char**argv){
 if(argc!=2||strcmp(argv[1],"--compare-original-input-neighbor-modes"))return 64;
 if(!exactSilverSource())return 65;
 void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",RTLD_NOW|RTLD_LOCAL);
 if(!lib)return 66;
 Factory maker=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
 void* iter=maker?maker():nullptr,*device=nullptr,*in=nullptr,*status=nullptr;
 if(!iter){dlclose(lib);return 67;}
 int32_t rc=((Next)(*(void***)iter)[3])(iter,&device);rel(iter);
 if(rc||!device){dlclose(lib);return 68;}
 rc=query(device,kInputIID,&in);
 if(rc||!in){rel(device);dlclose(lib);return 69;}
 rc=query(device,kStatusIID,&status);
 if(rc||!status){rel(in);rel(device);dlclose(lib);return 70;}
 void** iv=*(void***)in;void** sv=*(void***)status;
 unsigned positiveModes=0,attempted=0;
 for(unsigned m=0;m<sizeof(kModes)/sizeof(kModes[0]);++m){
     const Mode& entry=kModes[m];
     uint32_t supported=0;void* modeObject=nullptr;
     int32_t sup=((Support)iv[3])(in,entry.mode,kUYVY,0,&supported,&modeObject);
     rel(modeObject);
     printf("stage=fixed_input_candidate mode=%s fourcc=0x%08x "
            "support_rc=0x%08x support=%u source_hdmi_unchanged=1\n",
            entry.label,(unsigned)entry.mode,(unsigned)sup,(unsigned)supported);
     fflush(stdout);
     if(sup||!supported)continue;
     rc=((Enable)iv[6])(in,entry.mode,kUYVY,0);
     printf("stage=fixed_input_candidate mode=%s enable_rc=0x%08x\n",
             entry.label,(unsigned)rc);
     fflush(stdout);
     if(rc)continue;
     ++attempted;
     unsigned yes=0,stable=0;
     for(int sample=0;sample<12;++sample){
         bool locked=false;
         int32_t lrc=((GetFlag)sv[3])(status,0x7669736c,&locked);
         if(!lrc && locked){++yes;++stable;}else stable=0;
         printf("stage=fixed_input_candidate mode=%s sample=%d "
                "lock_rc=0x%08x locked=%s stable=%u\n",
                entry.label,sample,(unsigned)lrc,
                lrc==0?(locked?"YES":"NO"):"UNKNOWN",stable);
         fflush(stdout);
         if(stable>=2)break;
         if(sample<11)usleep(250000);
     }
     const int32_t disc=((NoArg)iv[7])(in);
     printf("stage=fixed_input_candidate mode=%s disable_rc=0x%08x "
            "saw_lock=%u stable=%u streams_started=0\n",
            entry.label,(unsigned)disc,yes,stable);
     if(stable>=2)++positiveModes;
     fflush(stdout);
     usleep(200000);
 }
 printf("stage=fixed_input_differential result=%s modes_probed=%u "
        "modes_locked=%u mac_source_kept_at_1080p30=1 "
        "input_only=1 frames_recorded=0\n",
        positiveModes?"MATCH_FOUND":"NONE_LOCKED",
        attempted,positiveModes);
 rel(status);rel(in);rel(device);dlclose(lib);
 return positiveModes?0:32;
}

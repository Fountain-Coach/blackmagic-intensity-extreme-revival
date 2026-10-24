// Original Intensity Extreme Sierra Desktop Video 10.4 HDMI OUT feedback
// diagnostic, deliberately read-only. No input or output enable, no
// video playback, no monitor reconfiguration. Query the HDMI sink EDID
// with bounded independent sizes (512/4096/8192) to distinguish a
// too-small buffer from genuinely unavailable/unsupported EDID.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysctl.h>

typedef void* (*Factory)();
typedef int32_t (*Next)(void*,void**);
typedef int32_t (*QI)(void*,CFUUIDBytes,void**);
typedef uint32_t (*Release)(void*);
typedef int32_t (*GetBytes)(void*,uint32_t,void*,uint32_t*);
typedef int32_t (*GetInt)(void*,uint32_t,int64_t*);
static const uint8_t statusIID[16]={0x5f,0x55,0x82,0x00,0x40,0x28,
  0x49,0xbc,0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static void release(void* p){if(p)((Release)(*(void***)p)[2])(p);}
static bool silver() {
 char model[64]={0};size_t n=sizeof(model);
 return !sysctlbyname("hw.model",model,&n,0,0)&&!strcmp(model,"Macmini5,3");
}
int main(int argc,char**argv){
 if(argc!=2||strcmp(argv[1],"--read-only"))return 64;
 if(!silver())return 20;
 void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                 RTLD_NOW|RTLD_LOCAL);
 if(!lib)return 21;
 Factory create=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
 void* it=create?create():0,*dev=0,*status=0;
 if(!it){dlclose(lib);return 22;}
 int32_t rc=((Next)(*(void***)it)[3])(it,&dev);release(it);
 if(rc||!dev){dlclose(lib);return 23;}
 CFUUIDBytes iid;memcpy(&iid,statusIID,16);
 rc=((QI)(*(void***)dev)[0])(dev,iid,&status);
 if(rc||!status){release(dev);dlclose(lib);return 24;}
 void** vt=*(void***)status;
 uint32_t sizes[]={512,4096,8192};
 for(unsigned i=0;i<3;i++){
   uint8_t buffer[8192]={0};
   uint32_t supplied=sizes[i], returned=supplied;
   rc=((GetBytes)vt[7])(status,0x65646964u,buffer,&returned);
   bool valid=false;unsigned bad=0,blocks=0,ext=0;
   if(rc==0&&returned>=128&&returned<=supplied &&returned%128==0) {
     static const uint8_t header[]={0,255,255,255,255,255,255,0};
     valid=!memcmp(buffer,header,8);blocks=returned/128;ext=buffer[126];
     for(unsigned b=0;b<blocks;b++){
       unsigned sum=0;for(unsigned j=0;j<128;j++)sum+=buffer[b*128+j];
       if(sum%256)bad++;
     }
     if(bad)valid=false;
   }
   printf("stage=hdmi_ddc_buffer requested_bytes=%u result_code=0x%08x "
          "returned_bytes=%u edid_valid=%u blocks=%u bad_checksums=%u "
          "extensions=%u optical_pixels=UNMEASURED\n",
          supplied,(unsigned)rc,returned,valid?1:0,blocks,bad,ext);
 }
 int64_t mode=0,flags=0;
 int32_t mrc=((GetInt)vt[4])(status,0x63766f6du,&mode);
 int32_t frc=((GetInt)vt[4])(status,0x63766f66u,&flags);
 printf("stage=hdmi_ddc_status current_output_mode_rc=0x%08x "
        "mode=0x%08x output_flags_rc=0x%08x flags=0x%08x "
        "monitor_pixels_verified=0 cables_untouched=1\n",
        (unsigned)mrc,(unsigned)(uint32_t)mode,(unsigned)frc,
        (unsigned)(uint32_t)flags);
 release(status);release(dev);dlclose(lib);
 return 0;
}

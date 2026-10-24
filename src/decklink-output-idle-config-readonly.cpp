// Original Sierra 10.12 + DeckLink 10.4: read-only output idle config audit.
// Device-specific values are inspected without modifying preferences/firmware.
// 'voio': black/last frame/desktop when inactive; 'dvom': default video mode;
// 'dvof': default output flags; 'cptm': capture passthrough mode.
// 'vocn' included to distinguish HDMI from other available outputs.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef void*(*Factory)();
typedef int32_t(*Next)(void*,void**);
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
typedef int32_t(*GetFlag)(void*,uint32_t,bool*);
typedef uint32_t(*Release)(void*);
static const uint8_t oldConfigIID[16]={
   0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,
   0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb
};
static void release(void* x){if(x)((Release)(*(void***)x)[2])(x);}
int main(int argc,char**argv) {
  if(argc!=2||strcmp(argv[1],"--read-only"))return 64;
  void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                  RTLD_NOW|RTLD_LOCAL);
  if(!lib)return 65;
  Factory f=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
  void* it=f?f():nullptr,*device=nullptr,*cfg=nullptr;
  if(!it){dlclose(lib);return 66;}
  int32_t rc=((Next)(*(void***)it)[3])(it,&device);
  release(it);
  if(rc||!device){dlclose(lib);return 67;}
  CFUUIDBytes id;memcpy(&id,oldConfigIID,16);
  rc=((QI)(*(void***)device)[0])(device,id,&cfg);
  printf("stage=original_idle_config interface_rc=0x%08x present=%d\n",
         (unsigned)rc,cfg!=nullptr);
  if(!rc&&cfg) {
    const uint32_t keys[]={0x766f696fu,0x64766f6du,0x64766f66u,
                            0x6370746du,0x766f636eu,0x7669636eu};
    const char* names[]={"idle_output_operation","default_output_mode",
        "default_output_flags","capture_passthrough","output_connection",
        "input_connection"};
    for(unsigned i=0;i<6;++i){
      int64_t val=0;
      int32_t result=((GetInt)(*(void***)cfg)[6])(cfg,keys[i],&val);
      printf("stage=original_idle_config key=%s rc=0x%08x "
             "value_fourcc_or_flags=0x%08x source_change=0\n",
             names[i],(unsigned)result,(unsigned)(uint32_t)val);
    }
    // On older devices `bvoc` may independently control whether HDMI OUT
    // shows black or incoming video during capture. It differs from idle
    // `voio=blac`; do not infer the two are equivalent.
    const uint32_t flagKeys[]={0x62766f63u,0x62797073u};
    const char* flagNames[]={"black_during_capture_bvoc",
                            "hardware_bypass_byps"};
    for(unsigned i=0;i<2;++i){
      bool value=false;
      int32_t flagRC=((GetFlag)(*(void***)cfg)[3])(cfg,flagKeys[i],&value);
      printf("stage=original_passthrough_flags key=%s rc=0x%08x "
             "value=%s used_as_test_signal=0 modified=0\n",
             flagNames[i],(unsigned)flagRC,
             flagRC==0?(value?"TRUE":"FALSE"):"UNSUPPORTED");
    }
  }
  release(cfg);release(device);dlclose(lib);
  puts("stage=original_idle_config modified=0 persisted=0 rebooted=0");
  return 0;
}

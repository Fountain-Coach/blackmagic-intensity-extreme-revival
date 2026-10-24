// 2026-10-24. Original silver Sierra Intensity Extreme physical output
// connector witness. READ-ONLY. HDMI OUT can be disconnected logically even
// if DeckLink accepts 1080p30 playback. Do not touch driver settings.
#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef void*(*Factory)();
typedef int32_t(*Next)(void*,void**);
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef uint32_t(*Release)(void*);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
static const uint8_t configIID[16]={
  0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb};
static const uint8_t attributesIID[16]={
  0xab,0xc1,0x18,0x43,0xd9,0x66,0x44,0xcb,0x96,0xe2,0xa1,0xcb,0x5d,0x31,0x35,0xc4};
static const uint8_t statusIID[16]={
  0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static void release(void* p){if(p)((Release)(*(void***)p)[2])(p);}
static int32_t query(void* p,const uint8_t id[16],void**out){
  CFUUIDBytes b;memcpy(&b,id,16);return ((QI)(*(void***)p)[0])(p,b,out);
}
int main(int argc,char**argv){
  if(argc!=2||strcmp(argv[1],"--read-only"))return 64;
  char model[64]={0};size_t n=sizeof(model);
  if(sysctlbyname("hw.model",model,&n,0,0)||strcmp(model,"Macmini5,3"))return 20;
  void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",RTLD_NOW|RTLD_LOCAL);
  if(!lib)return 21;
  Factory f=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
  void* it=f?f():0,*dev=0,*cfg=0,*attrs=0,*status=0;
  if(!it){dlclose(lib);return 22;}
  int32_t rc=((Next)(*(void***)it)[3])(it,&dev);release(it);
  if(rc||!dev){dlclose(lib);return 23;}
  int32_t c=query(dev,configIID,&cfg),a=query(dev,attributesIID,&attrs);
  int32_t ss=query(dev,statusIID,&status);
  if(c||!cfg){release(attrs);release(status);release(dev);dlclose(lib);return 24;}
  int64_t output=0,available=0,idle=0,idleMode=0,input=0,actualMode=0;
  int32_t outputRC=((GetInt)(*(void***)cfg)[6])(cfg,0x766f636eu,&output); // 'vocn'
  int32_t availRC=(attrs&&!a)?((GetInt)(*(void***)attrs)[4])(attrs,0x766f636eu,&available):a;
  int32_t idleRC=((GetInt)(*(void***)cfg)[6])(cfg,0x766f696fu,&idle); // 'voio'
  int32_t defaultRC=((GetInt)(*(void***)cfg)[6])(cfg,0x64766f6du,&idleMode); // 'dvom'
  int32_t inputRC=((GetInt)(*(void***)cfg)[6])(cfg,0x7669636eu,&input); // 'vicn'
  int32_t modeRC=(status&&!ss)?((GetInt)(*(void***)status)[4])(status,0x63766f6du,&actualMode):ss;
  printf("stage=original_output_connector config_query_rc=0x%08x "
         "current_output_rc=0x%08x selected_output=0x%08x hdmi_selected=%u hdmi_exclusive=%u "
         "capabilities_rc=0x%08x supported_output_mask=0x%08x "
         "hdmi_supported=%u component_supported=%u composite_supported=%u "
         "selected_input_rc=0x%08x selected_input=0x%08x "
         "idle_rc=0x%08x idle=0x%08x default_mode_rc=0x%08x default_mode=0x%08x "
         "current_mode_rc=0x%08x current_mode=0x%08x writes=0 streams=0\n",
         (unsigned)c,(unsigned)outputRC,(unsigned)(uint32_t)output,
         outputRC==0&&((uint32_t)output&2)?1:0,
         outputRC==0&&(uint32_t)output==2?1:0,
         (unsigned)availRC,(unsigned)(uint32_t)available,
         availRC==0&&(available&2)?1:0,availRC==0&&(available&8)?1:0,
         availRC==0&&(available&16)?1:0,
         (unsigned)inputRC,(unsigned)(uint32_t)input,
         (unsigned)idleRC,(unsigned)(uint32_t)idle,
         (unsigned)defaultRC,(unsigned)(uint32_t)idleMode,
         (unsigned)modeRC,(unsigned)(uint32_t)actualMode);
  release(status);release(attrs);release(cfg);release(dev);dlclose(lib);
  return outputRC==0?0:25;
}

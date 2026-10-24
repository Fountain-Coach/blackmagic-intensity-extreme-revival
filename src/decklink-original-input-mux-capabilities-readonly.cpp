// Original DeckLink 10.4 readonly video-input connector capabilities.
// Query supported connector mask via IDeckLinkAttributes and selected connector
// via IDeckLinkConfiguration; no writes, input streams or pixel access.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef void*(*Factory)();
typedef int32_t(*Next)(void*,void**);
typedef int32_t(*Query)(void*,CFUUIDBytes,void**);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
typedef uint32_t(*Release)(void*);
static const uint8_t configIID[16]={
  0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb
};
static const uint8_t attributesIID[16]={
  0xab,0xc1,0x18,0x43,0xd9,0x66,0x44,0xcb,0x96,0xe2,0xa1,0xcb,0x5d,0x31,0x35,0xc4
};
static void release(void* p) { if(p)((Release)(*(void***)p)[2])(p); }
static int32_t query(void* device,const uint8_t bytes[16],void** out) {
  CFUUIDBytes iid;memcpy(&iid,bytes,16);
  return ((Query)(*(void***)device)[0])(device,iid,out);
}
int main(int argc,char**argv) {
  if(argc!=2||strcmp(argv[1],"--read-only"))return 64;
  void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",RTLD_NOW|RTLD_LOCAL);
  if(!lib)return 20;
  Factory factory=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
  void* iter=factory?factory():0,*dev=0,*cfg=0,*attrs=0;
  if(!iter){dlclose(lib);return 21;}
  int32_t rc=((Next)(*(void***)iter)[3])(iter,&dev);release(iter);
  if(rc||!dev){dlclose(lib);return 22;}
  int32_t ar=query(dev,attributesIID,&attrs),cr=query(dev,configIID,&cfg);
  int64_t supported=0,selected=0;
  int32_t sr=attrs?((GetInt)(*(void***)attrs)[4])(attrs,0x7669636e,&supported):(int32_t)0x80004003;
  int32_t cr2=cfg?((GetInt)(*(void***)cfg)[6])(cfg,0x7669636e,&selected):(int32_t)0x80004003;
  printf("stage=hdmi_input_mux_caps attributes_interface_rc=0x%08x "
         "supported_query_rc=0x%08x supported_mask=0x%08x "
         "supports_hdmi=%u supports_component=%u "
         "config_interface_rc=0x%08x selected_query_rc=0x%08x selected_input=0x%08x "
         "changed=0 streams_started=0\n",
         (unsigned)ar,(unsigned)sr,(unsigned)(uint32_t)supported,
         sr==0&&((uint32_t)supported&2)?1:0,
         sr==0&&((uint32_t)supported&8)?1:0,
         (unsigned)cr,(unsigned)cr2,(unsigned)(uint32_t)selected);
  release(attrs);release(cfg);release(dev);dlclose(lib);
  return sr==0&&cr2==0?0:24;
}

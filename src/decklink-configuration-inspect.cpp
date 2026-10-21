#include <initializer_list>
// Fountain Coach: read-only video input/output configuration inspection.
// Requires installed legacy DeckLinkAPI.framework, no SDK headers.
// Does not set or persist any settings.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef void*(*Factory)(void);
typedef int32_t(*Query)(void*,CFUUIDBytes,void**);
typedef int32_t(*Next)(void*,void**);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
typedef uint32_t(*Release)(void*);
static const unsigned char iid[16]={0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb};
int main(){
 void* handle=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",RTLD_NOW|RTLD_LOCAL);
 if(!handle){fprintf(stderr,"error=dlopen\n");return 20;}
 auto factory=(Factory)dlsym(handle,"CreateDeckLinkIteratorInstance");
 if(!factory)return 21;
 void* iter=factory();if(!iter)return 22;
 void* dev=nullptr;
 auto vt=*(void***)iter;int32_t st=((Next)vt[3])(iter,&dev);
 ((Release)vt[2])(iter);
 if(st||!dev){printf("device=unavailable status=0x%08x\n",(unsigned)st);return 23;}
 CFUUIDBytes u;memcpy(&u,iid,16);
 void* cfg=nullptr;auto dvt=*(void***)dev;
 st=((Query)dvt[0])(dev,u,&cfg);
 printf("configuration_interface_status=0x%08x present=%d\n",(unsigned)st,cfg!=nullptr);
 if(!st&&cfg){
  auto cvt=*(void***)cfg;
  for(uint32_t k: {0x7669636eU,0x766f636eU}){
   int64_t value=-1;
   int32_t rc=((GetInt)cvt[6])(cfg,k,&value);
   printf("cfg=%s status=0x%08x value=0x%llx\n",k==0x7669636eU?"input":"output",(unsigned)rc,(unsigned long long)value);
  }
  ((Release)cvt[2])(cfg);
 }
 ((Release)dvt[2])(dev);dlclose(handle);return st?24:0;
}

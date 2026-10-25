/*
 * Fountain Video emergency quiesce: stop original Intensity Extreme video
 * output without ever enabling a stream.
 *
 * For owner-requested shutdown of an unwanted Panasonic on/off loop.
 * Does not change HDMI IN, launch a video stream, edit firmware, or kexts.
 * DisableVideoOutput is an original documented DeckLink method.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <sys/sysctl.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef void* (*Factory)(void);
typedef int32_t (*Next)(void*,void**);
typedef int32_t (*Query)(void*,CFUUIDBytes,void**);
typedef int32_t (*GetModelName)(void*,CFStringRef*);
typedef int32_t (*GetInt)(void*,uint32_t,int64_t*);
typedef int32_t (*Disable)(void*);
typedef uint32_t (*Release)(void*);

static const uint8_t output_id[]={
 0xcc,0x5c,0x8a,0x6e,0x3f,0x2f,0x4b,0x3a,
 0x87,0xea,0xfd,0x78,0xaf,0x30,0x05,0x64
};
static const uint8_t attributes_id[]={
 0xab,0xc1,0x18,0x43,0xd9,0x66,0x44,0xcb,
 0x96,0xe2,0xa1,0xcb,0x5d,0x31,0x35,0xc4
};
static void release_ref(void* p){if(p)((Release)(*(void***)p)[2])(p);}
static int safe_host(void){
 char model[64]={0},kernel[64]={0};
 size_t ml=sizeof(model),kl=sizeof(kernel);
 return !sysctlbyname("hw.model",model,&ml,NULL,0) &&
        !sysctlbyname("kern.osrelease",kernel,&kl,NULL,0) &&
        !strcmp(model,"Macmini5,3") && !strncmp(kernel,"16.",3);
}
int main(void){
 if(!safe_host()){
    puts("stage=quiesce result=WRONG_HOST");return 20;
 }
 void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                 RTLD_NOW|RTLD_LOCAL);
 if(!lib){puts("stage=quiesce result=FRAMEWORK_MISSING");return 21;}
 Factory make=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
 void* it=make?make():NULL;
 if(!it){dlclose(lib);return 22;}
 void* device=NULL;
 int32_t rc=((Next)(*(void***)it)[3])(it,&device);
 release_ref(it);
 if(rc||!device){dlclose(lib);return 23;}
 CFStringRef cf=NULL;
 rc=((GetModelName)(*(void***)device)[3])(device,&cf);
 char model[100]={0};
 int ok=cf && CFStringGetCString(cf,model,sizeof(model),kCFStringEncodingUTF8);
 if(cf)CFRelease(cf);
 if(rc||!ok||strcmp(model,"Intensity Extreme")){
     puts("stage=quiesce result=NOT_ORIGINAL_INTENSITY");
     release_ref(device);dlclose(lib);return 24;
 }
 CFUUIDBytes attr_id;memcpy(&attr_id,attributes_id,16);
 void* attrs=NULL;rc=((Query)(*(void***)device)[0])(device,attr_id,&attrs);
 if(rc||!attrs){
     release_ref(device);dlclose(lib);return 25;
 }
 int64_t busy=-1;
 rc=((GetInt)(*(void***)attrs)[4])(attrs,0x64627374u,&busy);
 release_ref(attrs);
 printf("stage=quiesce busy_rc=0x%08x busy=%lld\n",
        (unsigned)rc,(long long)busy);
 if(rc||busy!=0){
    puts("stage=quiesce result=DEVICE_NOT_IDLE");
    release_ref(device);dlclose(lib);return 26;
 }
 CFUUIDBytes out_id;memcpy(&out_id,output_id,16);
 void* out=NULL;
 rc=((Query)(*(void***)device)[0])(device,out_id,&out);
 if(rc||!out){
    puts("stage=quiesce result=NO_OUTPUT_INTERFACE");
    release_ref(device);dlclose(lib);return 27;
 }
 rc=((Disable)(*(void***)out)[7])(out);
 printf("stage=quiesce DisableVideoOutput_rc=0x%08x\n",(unsigned)rc);
 release_ref(out);
 release_ref(device);
 dlclose(lib);
 printf("stage=quiesce result=%s no_frames_sent=1 no_enable_called=1"
        " physical_HDMI_voltage=UNMEASURED\n",rc==0?"SUCCESS":"FAILED");
 return rc==0?0:28;
}

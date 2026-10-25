/*
 * EMERGENCY PHYSICAL HDMI-OUT QUIET MODE for the original Intensity Extreme.
 *
 * Owner-requested: STOP Panasonic repeated on/off cycling by disabling
 * Blackmagic HDMI output in device settings, not by unplugging hardware.
 *
 * Exact original Sierra Macmini5,3 and original Intensity Extreme only.
 * Requires idle device, original HDMI input=0x2, original HDMI+component
 * output selection=0xA. Changes ONLY the video output connection to
 * component-only=0x8. Persists the setting via original DeckLink
 * WriteConfigurationToPreferences, then reads back and verifies from a
 * fresh device interface, with HDMI bit 0x2 absent.
 *
 * No video input changes, driver changes, screen mode changes, frame streams,
 * PCI register accesses, firmware writes, or reboot. Fails closed.
 *
 * Deliberately NOT a general-purpose configuration editor.
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
typedef int32_t (*ReadInt)(void*,uint32_t,int64_t*);
typedef int32_t (*SetInt)(void*,uint32_t,int64_t);
typedef int32_t (*Persist)(void*);
typedef uint32_t (*Release)(void*);

static const uint8_t cfg_id[16]={
  0x1e,0x69,0xfc,0xf6,0x42,0x03,0x49,0x36,
  0x80,0x76,0x2a,0x9f,0x4c,0xfd,0x50,0xcb
};
static const uint8_t attrs_id[16]={
  0xab,0xc1,0x18,0x43,0xd9,0x66,0x44,0xcb,
  0x96,0xe2,0xa1,0xcb,0x5d,0x31,0x35,0xc4
};
static const uint32_t INPUT_KEY=0x7669636eu;
static const uint32_t OUTPUT_KEY=0x766f636eu;
static const int64_t ORIGINAL_INPUT=2, ORIGINAL_OUTPUT=10;
static const int64_t SAFE_OUTPUT=8;
static void release_ref(void* p){
    if(p)((Release)(*(void***)p)[2])(p);
}
static int original_sierra(void){
    char model[64]={0},kernel[64]={0};
    size_t ml=sizeof(model),kl=sizeof(kernel);
    return !sysctlbyname("hw.model",model,&ml,NULL,0) &&
           !sysctlbyname("kern.osrelease",kernel,&kl,NULL,0) &&
           !strcmp(model,"Macmini5,3") && !strncmp(kernel,"16.",3);
}
static int32_t config_int(void* cfg,uint32_t key,int64_t* value){
    return ((ReadInt)(*(void***)cfg)[6])(cfg,key,value);
}
static int get_config(void* dev,void** out) {
    CFUUIDBytes iid;memcpy(&iid,cfg_id,16);
    *out=NULL;
    return ((Query)(*(void***)dev)[0])(dev,iid,out);
}
static int get_busy(void* dev,int64_t* result){
    CFUUIDBytes iid;memcpy(&iid,attrs_id,16);
    void* attr=NULL;
    int32_t rc=((Query)(*(void***)dev)[0])(dev,iid,&attr);
    if(rc||!attr)return 0;
    int32_t busyrc=((ReadInt)(*(void***)attr)[4])(
       attr,0x64627374u,result);
    release_ref(attr);
    return busyrc==0;
}
static int exact_model(void* dev){
    CFStringRef cf=NULL;
    int32_t rc=((GetModelName)(*(void***)dev)[3])(dev,&cf);
    if(rc||!cf)return 0;
    char name[128]={0};
    int ok=CFStringGetCString(cf,name,sizeof(name),kCFStringEncodingUTF8);
    CFRelease(cf);
    printf("stage=stop_hdmi model=%s\n",name);
    return ok && !strcmp(name,"Intensity Extreme");
}
int main(int argc,char**argv) {
    if(argc!=2||strcmp(argv[1],"--disable-hdmi-out")){
        puts("usage: fountain-stop-hdmi-output --disable-hdmi-out");
        return 64;
    }
    if(!original_sierra()){
        puts("stage=stop_hdmi result=WRONG_HOST");
        return 20;
    }
    void* handle=dlopen(
        "/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
        RTLD_NOW|RTLD_LOCAL);
    if(!handle){puts("stage=stop_hdmi result=FRAMEWORK_MISSING");return 21;}
    Factory make=(Factory)dlsym(handle,"CreateDeckLinkIteratorInstance");
    void* iter=make?make():NULL;
    if(!iter){dlclose(handle);return 22;}
    void* dev=NULL;
    int32_t rc=((Next)(*(void***)iter)[3])(iter,&dev);
    release_ref(iter);
    if(rc||!dev){dlclose(handle);return 23;}
    if(!exact_model(dev)){
        puts("stage=stop_hdmi result=WRONG_DEVICE");
        release_ref(dev);dlclose(handle);return 24;
    }
    int64_t busy=-1;
    if(!get_busy(dev,&busy)||busy!=0){
        printf("stage=stop_hdmi result=DEVICE_NOT_IDLE busy=%lld\n",
               (long long)busy);
        release_ref(dev);dlclose(handle);return 25;
    }
    void* cfg=NULL;
    rc=get_config(dev,&cfg);
    if(rc||!cfg){release_ref(dev);dlclose(handle);return 26;}
    int64_t input=-1,output=-1;
    int32_t irc=config_int(cfg,INPUT_KEY,&input);
    int32_t orc=config_int(cfg,OUTPUT_KEY,&output);
    printf("stage=stop_hdmi original_input_status=0x%08x value=0x%llx"
           " original_output_status=0x%08x value=0x%llx\n",
           (unsigned)irc,(unsigned long long)input,
           (unsigned)orc,(unsigned long long)output);
    fflush(stdout);
    if(irc || orc || input!=ORIGINAL_INPUT ||
       (output!=ORIGINAL_OUTPUT && output!=SAFE_OUTPUT)){
        puts("stage=stop_hdmi result=UNKNOWN_CONFIGURATION_NO_WRITE");
        release_ref(cfg);release_ref(dev);dlclose(handle);return 27;
    }
    if(output==SAFE_OUTPUT){
        puts("stage=stop_hdmi already_component_only=1"
             " HDMI_bit=0 no_write=1");
    }else{
        rc=((SetInt)(*(void***)cfg)[5])(cfg,OUTPUT_KEY,SAFE_OUTPUT);
        printf("stage=stop_hdmi set_component_only_rc=0x%08x\n",(unsigned)rc);
        fflush(stdout);
        if(rc){
            release_ref(cfg);release_ref(dev);dlclose(handle);
            return 28;
        }
    }
    int64_t after=-1,after_input=-1;
    int32_t readrc=config_int(cfg,OUTPUT_KEY,&after);
    int32_t inputrc=config_int(cfg,INPUT_KEY,&after_input);
    printf("stage=stop_hdmi immediate_output_rc=0x%08x output=0x%llx"
           " immediate_input_rc=0x%08x input=0x%llx\n",
           (unsigned)readrc,(unsigned long long)after,
           (unsigned)inputrc,(unsigned long long)after_input);
    fflush(stdout);
    if(readrc||inputrc||after!=SAFE_OUTPUT||after_input!=ORIGINAL_INPUT){
        puts("stage=stop_hdmi result=CHANGE_NOT_VERIFIED");
        release_ref(cfg);release_ref(dev);dlclose(handle);return 29;
    }
    if(output!=SAFE_OUTPUT){
        rc=((Persist)(*(void***)cfg)[11])(cfg);
        printf("stage=stop_hdmi save_to_preferences_rc=0x%08x\n",(unsigned)rc);
        fflush(stdout);
        if(rc){
            puts("stage=stop_hdmi result=PERSISTENCE_NOT_CONFIRMED");
            release_ref(cfg);release_ref(dev);dlclose(handle);return 30;
        }
    }
    release_ref(cfg);
    cfg=NULL;
    rc=get_config(dev,&cfg);
    int64_t confirm=-1,confirm_input=-1;
    int32_t confirmrc=(rc||!cfg)?(int32_t)-1:config_int(cfg,OUTPUT_KEY,&confirm);
    int32_t confirmirc=(rc||!cfg)?(int32_t)-1:config_int(cfg,INPUT_KEY,&confirm_input);
    printf("stage=stop_hdmi fresh_output_rc=0x%08x output=0x%llx"
           " fresh_input_rc=0x%08x input=0x%llx\n",
           (unsigned)confirmrc,(unsigned long long)confirm,
           (unsigned)confirmirc,(unsigned long long)confirm_input);
    if(cfg)release_ref(cfg);
    release_ref(dev);
    dlclose(handle);
    if(confirmrc||confirmirc||confirm!=SAFE_OUTPUT||
       confirm_input!=ORIGINAL_INPUT){
        puts("stage=stop_hdmi result=INDEPENDENT_REOPEN_NOT_CONFIRMED");
        return 31;
    }
    puts("stage=stop_hdmi result=PASS HDMI_OUT=DESELECTED"
         " component_only=1 HDMI_IN=UNCHANGED"
         " no_capture=1 no_playback=1 no_firmware=1");
    return 0;
}

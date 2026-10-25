/*
 * Fountain Coach original Intensity Extreme output-status witness.
 *
 * Strictly read-only. The original Sierra Desktop Video 10.4
 * CDeckLinkStatus::GetBytes is NOT the modern EDID-returning implementation.
 * Its exact x86_64 10.4 implementation at offset 0x1f3e4:
 *
 *    movq $0, (%rdx)
 *    movl $0x1000, (%rcx)
 *    movl $0x80000003, %eax
 *
 * That is a constant NOT_IMPLEMENTED stub. Passing a null third argument
 * crashed one isolated probe on 2026-10-25. DO NOT add an EDID/GetBytes
 * invocation here, even to query the required buffer size.
 *
 * Only query GetInt, which has been witnessed returning a real mode code.
 * No output enable, capture, configuration changes, PCI MMIO or firmware.
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
typedef int32_t (*ReadInteger)(void*,uint32_t,int64_t*);
typedef uint32_t (*Release)(void*);
static void release_ref(void* p){
    if(p)((Release)(*(void***)p)[2])(p);
}
static int original_host(void){
    char model[64]={0},kernel[64]={0};
    size_t m=sizeof(model),k=sizeof(kernel);
    return !sysctlbyname("hw.model",model,&m,NULL,0) &&
           !sysctlbyname("kern.osrelease",kernel,&k,NULL,0) &&
           !strcmp(model,"Macmini5,3") && !strncmp(kernel,"16.",3);
}
static const uint8_t kStatusIID[16]={
 0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,
 0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46
};
static void value(void* status,uint32_t key,const char* name){
    int64_t n=-1;
    int32_t rc=((ReadInteger)(*(void***)status)[4])(status,key,&n);
    printf("stage=video_output_status key=%s rc=0x%08x"
           " value=0x%08x value_decimal=%lld\n",
           name,(unsigned)rc,(unsigned)(uint32_t)n,(long long)n);
}
int main(void){
    if(!original_host()){
        puts("stage=video_output_status result=BLOCKED_WRONG_ORIGINAL_HOST");
        return 20;
    }
    void* library=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                         RTLD_NOW|RTLD_LOCAL);
    if(!library){puts("stage=video_output_status result=NO_LEGACY_FRAMEWORK");return 21;}
    Factory make=(Factory)dlsym(library,"CreateDeckLinkIteratorInstance");
    void* it=make?make():NULL;
    if(!it){dlclose(library);return 22;}
    void* device=NULL;
    int32_t rc=((Next)(*(void***)it)[3])(it,&device);
    release_ref(it);
    if(rc||!device){dlclose(library);return 23;}
    CFUUIDBytes id;memcpy(&id,kStatusIID,16);
    void* status=NULL;
    rc=((Query)(*(void***)device)[0])(device,id,&status);
    printf("stage=video_output_status status_interface_rc=0x%08x"
           " available=%d\n",(unsigned)rc,status!=NULL);
    if(!rc && status){
        value(status,0x63766f6dU,"current_output_mode");
        value(status,0x6f706978U,"last_output_pixel_format");
        value(status,0x63766f66U,"current_output_flags");
        value(status,0x62757379U,"busy");
        value(status,0x6376696dU,"current_input_mode");
        puts("stage=video_output_status result=READ_ONLY_COMPLETE"
             " panasonic_hdmi_edid=UNAVAILABLE_ON_LEGACY_RUNTIME"
             " on_wire_pixels=NOT_MEASURED");
    }else{
        puts("stage=video_output_status result=STATUS_NOT_EXPOSED"
             " output_mode=UNKNOWN");
    }
    release_ref(status);
    release_ref(device);
    dlclose(library);
    return 0;
}

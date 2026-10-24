// Read-only original Intensity Extreme HDMI OUT EDID / sink probe.
// Distinguishes output API acceptance from independent sink detection.
// Uses the installed vendor 10.4 IDeckLinkStatus virtual interface.
// Never stores an EDID, turns on output, or opens/records private pixels.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef void*(*Factory)();
typedef int32_t(*Next)(void*,void**);
typedef int32_t(*QI)(void*,CFUUIDBytes,void**);
typedef uint32_t(*Release)(void*);
typedef int32_t(*GetBytes)(void*,uint32_t,void*,uint32_t*);
typedef int32_t(*GetInt)(void*,uint32_t,int64_t*);
static const uint8_t kStatusIID[16]={0x5f,0x55,0x82,0x00,0x40,0x28,
                                     0x49,0xbc,0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46};
static void release(void* object) {
    if(object)((Release)(*(void***)object)[2])(object);
}
int main(int argc,char** argv) {
    if(argc!=2||strcmp(argv[1],"--sink-readonly"))return 64;
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                     RTLD_NOW|RTLD_LOCAL);
    if(!lib)return 20;
    Factory factory=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    if(!factory){dlclose(lib);return 21;}
    void* it=factory();
    if(!it){dlclose(lib);return 22;}
    void* dev=nullptr;
    const int32_t next=((Next)(*(void***)it)[3])(it,&dev);
    release(it);
    if(next||!dev){dlclose(lib);return 23;}
    CFUUIDBytes iid;memcpy(&iid,kStatusIID,16);
    void* status=nullptr;
    const int32_t q=((QI)(*(void***)dev)[0])(dev,iid,&status);
    if(q||!status){
        printf("stage=original_hdmi_sink status_interface=unavailable rc=0x%08x\n",
               (unsigned)q);
        release(dev);dlclose(lib);return 24;
    }
    void** vt=*(void***)status;
    uint8_t raw[512]={0};
    uint32_t count=sizeof(raw);
    const int32_t rc=((GetBytes)vt[7])(status,0x65646964u,raw,&count);
    printf("stage=original_hdmi_sink edid_getbytes_rc=0x%08x bytes=%u "
           "optical_monitor_pixels_measured=0\n",(unsigned)rc,count);
    if(rc==0 && count>=128 && count<=sizeof(raw)) {
        const uint8_t reference[8]={0,255,255,255,255,255,255,0};
        unsigned invalid=memcmp(raw,reference,8)?1u:0u;
        unsigned blocks=count/128;
        for(unsigned b=0;b<blocks;++b){
            unsigned sum=0;
            for(unsigned i=0;i<128;++i)sum+=raw[b*128+i];
            if(sum%256)++invalid;
        }
        printf("stage=original_hdmi_sink edid_present=1 blocks=%u "
               "invalid_header_or_checksum=%u extension_count=%u "
               "sink_electrical_feedback=reported "
               "optical_content_verified=0\n",
               blocks,invalid,raw[126]);
    } else {
        printf("stage=original_hdmi_sink edid_present=unknown "
               "legacy_driver_may_not_implement_status=1\n");
    }
    // Historical device current output mode may not be available.
    int64_t mode=0;
    int32_t modeRc=((GetInt)vt[4])(status,0x63766f6du,&mode); // 'cvom'
    printf("stage=original_hdmi_output_mode status_rc=0x%08x value=0x%llx "
           "not_optical_evidence=1\n",(unsigned)modeRc,(unsigned long long)mode);
    release(status);release(dev);dlclose(lib);
    return 0;
}

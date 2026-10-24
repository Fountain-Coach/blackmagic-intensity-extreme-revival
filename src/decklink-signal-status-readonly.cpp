// Read-only Sierra DeckLink Status inspection. NEVER calls Enable/Start/Stop.
// Returns diagnostic state only; a reported lock is not captured-frame acceptance.
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef void* (*Factory)();
typedef int32_t (*Query)(void*, CFUUIDBytes, void**);
typedef int32_t (*Next)(void*, void**);
typedef int32_t (*GetFlag)(void*, uint32_t, bool*);
typedef int32_t (*GetInt)(void*, uint32_t, int64_t*);
typedef uint32_t (*Release)(void*);
static const unsigned char kStatusIID[16] = {
    0x5f,0x55,0x82,0x00,0x40,0x28,0x49,0xbc,
    0xbe,0xac,0xdb,0x3f,0xa4,0xa9,0x6e,0x46
};
int main() {
    void* lib=dlopen("/Library/Frameworks/DeckLinkAPI.framework/DeckLinkAPI",
                    RTLD_NOW|RTLD_LOCAL);
    if (!lib) { puts("stage=readonly_status result=framework_unavailable"); return 20; }
    Factory factory=(Factory)dlsym(lib,"CreateDeckLinkIteratorInstance");
    if (!factory) { dlclose(lib); return 21; }
    void* iterator=factory();
    if (!iterator) { dlclose(lib); return 22; }
    void** iterV=*(void***)iterator;
    void* device=nullptr;
    int32_t rc=((Next)iterV[3])(iterator,&device);
    ((Release)iterV[2])(iterator);
    if (rc || !device) {
        printf("stage=readonly_status result=device_unavailable rc=0x%08x\n",
               (unsigned)rc);
        dlclose(lib); return 23;
    }
    void** devV=*(void***)device;
    CFUUIDBytes iid; memcpy(&iid,kStatusIID,16);
    void* status=nullptr;
    rc=((Query)devV[0])(device,iid,&status);
    printf("stage=readonly_status status_interface=0x%08x present=%d\n",
           (unsigned)rc,status!=nullptr);
    int code=0;
    if (!rc && status) {
        void** stV=*(void***)status;
        bool locked=false;
        int64_t detected=0;
        int32_t lrc=((GetFlag)stV[3])(status,0x7669736c,&locked);
        int32_t drc=((GetInt)stV[4])(status,0x6476696d,&detected);
        printf("stage=readonly_input_signal result=observed queried_without_streams=1 "
               "lock_query_rc=0x%08x locked=%d mode_query_rc=0x%08x "
               "mode=0x%llx real_frame_verified=0\n",
               (unsigned)lrc,locked?1:0,(unsigned)drc,
               (unsigned long long)detected);
        ((Release)stV[2])(status);
    } else code=24;
    ((Release)devV[2])(device);
    dlclose(lib);
    puts("stage=safety vendor_input_enable=0 start_streams=0 framebuffer_read=0 "
         "vendor_driver_changes=0 pci_mmio=0 firmware_update=0");
    return code;
}

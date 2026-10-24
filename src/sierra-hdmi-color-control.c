/*
 * Sierra / original Intensity Extreme HDMI selected colour-mode experiment.
 * In current source rig: silver Macmini5,3 HDMI OUT -> BMD HDMI IN;
 * original Intensity HDMI OUT empty; DeckLink driver 10.4.
 *
 * Run only with separately armed host+external watchdog.
 * --inspect     : no changes; two independent service access paths
 * --set422      : nonpersistent driver parameter write (no CommitParameters)
 * --restore444  : idempotent restore original cmod=0x100 (no CommitParameters)
 * No display-resolution writes or HDMI input streams.
 *
 * IMPORTANT: IODisplay selected mode is not direct HDMI wire AVI InfoFrame data.
 * Merely changing cmod and seeing the new value DOES NOT prove BMD receiver lock.
 */
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
    kBMDVendor=8961, kBMDProduct=0,
    kYCbCr422=0x10, kYCbCr444=0x100,
    kRGB=0x1
};
static int read_number(io_service_t obj, CFStringRef key, uint32_t* dst) {
    CFTypeRef value=IORegistryEntryCreateCFProperty(obj,key,kCFAllocatorDefault,0);
    if(!value)return 0;
    int64_t n=0;
    int ok=CFGetTypeID(value)==CFNumberGetTypeID() &&
           CFNumberGetValue((CFNumberRef)value,kCFNumberSInt64Type,&n);
    if(ok)*dst=(uint32_t)n;
    CFRelease(value);
    return ok;
}
static io_service_t get_exact_bmd_display(void) {
    CFMutableDictionaryRef match=IOServiceMatching("AppleDisplay");
    if(!match)return IO_OBJECT_NULL;
    io_iterator_t list=IO_OBJECT_NULL;
    if(IOServiceGetMatchingServices(kIOMasterPortDefault,match,&list)!=KERN_SUCCESS)
        return IO_OBJECT_NULL;
    io_service_t obj=IO_OBJECT_NULL;
    io_service_t found=IO_OBJECT_NULL;
    unsigned count=0;
    while((obj=IOIteratorNext(list))!=IO_OBJECT_NULL){
        uint32_t vendor=0,product=UINT32_MAX;
        if(read_number(obj,CFSTR("DisplayVendorID"),&vendor) &&
           read_number(obj,CFSTR("DisplayProductID"),&product) &&
           vendor==kBMDVendor && product==kBMDProduct){
            ++count;
            if(!found)found=obj;
            else IOObjectRelease(obj);
        }else IOObjectRelease(obj);
    }
    IOObjectRelease(list);
    if(count!=1){
        if(found)IOObjectRelease(found);
        return IO_OBJECT_NULL;
    }
    return found;
}
static int selected(io_service_t obj, uint32_t *value, const char* kind) {
    SInt32 val=0,lo=0,hi=0;
    IOReturn status=IODisplayGetIntegerRangeParameter(obj,0,CFSTR("cmod"),
                                                      &val,&lo,&hi);
    printf("stage=color_read path=%s status=0x%08x value=0x%08x\n",
           kind,(unsigned)status,(unsigned)(uint32_t)val);
    if(status)return 0;
    *value=(uint32_t)val;
    return 1;
}
static int capabilities(io_service_t obj, uint32_t *supported) {
    SInt32 v=0,lo=0,hi=0;
    IOReturn rc=IODisplayGetIntegerRangeParameter(obj,0,CFSTR("colr"),&v,&lo,&hi);
    printf("stage=supported_color_modes status=0x%08x value=0x%08x rgb=%d ycbcr422=%d ycbcr444=%d\n",
           (unsigned)rc,(unsigned)(uint32_t)v,!!(v & kRGB),
           !!(v & kYCbCr422),!!(v & kYCbCr444));
    if(rc)return 0;
    *supported=(uint32_t)v;
    return 1;
}
int main(int argc,char**argv) {
    if(argc!=2 ||
       (strcmp(argv[1],"--inspect") &&
        strcmp(argv[1],"--set422") &&
        strcmp(argv[1],"--restore444"))){
        puts("usage: sierra-hdmi-color-control --inspect|--set422|--restore444");
        return 64;
    }
    const int write=strcmp(argv[1],"--inspect")!=0;
    const uint32_t target=strcmp(argv[1],"--set422")==0?kYCbCr422:kYCbCr444;
    char model[64]={0},release[64]={0};
    size_t ml=sizeof(model),rl=sizeof(release);
    if(sysctlbyname("hw.model",model,&ml,NULL,0) ||
       sysctlbyname("kern.osrelease",release,&rl,NULL,0) ||
       strcmp(model,"Macmini5,3") || strncmp(release,"16.",3)){
        puts("stage=guard result=WRONG_HOST");return 20;
    }
    io_service_t bmd=get_exact_bmd_display();
    uint32_t reg_value=0;
    const int registry_valid=(bmd && selected(bmd,&reg_value,"AppleDisplay"));
    CGDirectDisplayID ids[16]={0};uint32_t n=0;
    int bmd_online=CGGetOnlineDisplayList(16,ids,&n)==kCGErrorSuccess &&
                   n==1 && CGDisplayVendorNumber(ids[0])==kBMDVendor &&
                   CGDisplayModelNumber(ids[0])==kBMDProduct;
    io_service_t quartz=bmd_online?CGDisplayIOServicePort(ids[0]):IO_OBJECT_NULL;
    uint32_t quartz_value=0;
    const int quartz_valid=(quartz && selected(quartz,&quartz_value,"CGDisplay"));
    printf("stage=source_services bmd_apple_display=%d bmd_quartz_online=%d registry_api_read=%d quartz_api_read=%d\n",
           bmd!=IO_OBJECT_NULL,bmd_online,registry_valid,quartz_valid);
    if(!registry_valid&&!quartz_valid){
        puts("stage=color_control result=NO_READABLE_BMD_DISPLAY");
        if(bmd)IOObjectRelease(bmd);return 21;
    }
    io_service_t chosen=quartz_valid?quartz:bmd;
    uint32_t current=quartz_valid?quartz_value:reg_value,supported=0;
    if(!capabilities(chosen,&supported)){
        if(bmd)IOObjectRelease(bmd);return 22;
    }
    if(!write){
        puts("stage=color_control result=READ_ONLY actual_wire_encoding=UNVERIFIED");
        if(bmd)IOObjectRelease(bmd);return 0;
    }
    if(!(supported & target)){
        puts("stage=color_control result=TARGET_UNSUPPORTED");
        if(bmd)IOObjectRelease(bmd);return 23;
    }
    if(!bmd_online){
        puts("stage=color_control result=DISPLAY_OFFLINE_WRITE_BLOCKED");
        if(bmd)IOObjectRelease(bmd);return 24;
    }
    CGDisplayModeRef mode=CGDisplayCopyDisplayMode(ids[0]);
    if(!mode){
        if(bmd)IOObjectRelease(bmd);return 25;
    }
    double hz=CGDisplayModeGetRefreshRate(mode);
    size_t w=CGDisplayModeGetWidth(mode),h=CGDisplayModeGetHeight(mode);
    CGDisplayModeRelease(mode);
    if(w!=1920 || h!=1080 || hz<29.99 || hz>30.01){
        puts("stage=color_control result=WRONG_BASELINE_DISPLAY_MODE");
        if(bmd)IOObjectRelease(bmd);return 26;
    }
    // Only exact original cmod values accepted on live fixture.
    if(current!=kYCbCr422 && current!=kYCbCr444){
        printf("stage=color_control result=UNKNOWN_CURRENT_COLOR value=0x%08x\n",current);
        if(bmd)IOObjectRelease(bmd);return 27;
    }
    if(current==target){
        printf("stage=color_control result=ALREADY_TARGET value=0x%08x writes=0\n",target);
        if(bmd)IOObjectRelease(bmd);return 0;
    }
    printf("stage=color_write source_mode=1080p30 original=0x%08x target=0x%08x"
           " commit_parameters=0\n",current,target);
    fflush(stdout);
    IOReturn rc=IODisplaySetIntegerParameter(chosen,0,CFSTR("cmod"),(SInt32)target);
    printf("stage=color_write rc=0x%08x\n",(unsigned)rc);
    fflush(stdout);
    usleep(350000);
    uint32_t finalval=0;
    int verified=selected(chosen,&finalval,"post_write");
    if(bmd)IOObjectRelease(bmd);
    printf("stage=color_write result=%s requested=0x%08x readback=0x%08x"
           " commit_called=0 video_streams=0 mode_switches=0\n",
           (rc==0 && verified && finalval==target)?"APPLIED":"NOT_VERIFIED",
           target,finalval);
    return rc==0&&verified&&finalval==target?0:32;
}

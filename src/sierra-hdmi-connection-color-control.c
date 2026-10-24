/*
 * Read selected BMD HDMI source colour (cmod) and set the *connection*
 * colour control (cyuv) on the original Sierra Macmini5,3.
 *
 * Old, rejected experiment wrote cmod, a reported selected-mode property.
 * Correct documented connection attribute: kConnectionColorMode = 'cyuv'.
 * Separate restore watchdog + external SSH fallback required.
 *
 * --inspect: read only
 * --set422: DISABLED: IOGraphics rewrites non-automatic requests to RGB
 * --restore444: restore original cyuv=auto and confirm original cmod=0x100
 *
 * Never calls IODisplayCommitParameters, SetDisplayMode or DeckLink streams.
 * Actual HDMI wire encoding still cannot be independently certified.
 */
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { BMD_VENDOR=8961,BMD_PRODUCT=0,
       COLOR_422=0x10,COLOR_444=0x100,COLOR_AUTO=0x10000000 };
static int num(io_service_t service,CFStringRef key,uint32_t* out){
    CFTypeRef x=IORegistryEntryCreateCFProperty(service,key,kCFAllocatorDefault,0);
    if(!x)return 0;
    int64_t v=0;
    int ok=CFGetTypeID(x)==CFNumberGetTypeID() &&
           CFNumberGetValue((CFNumberRef)x,kCFNumberSInt64Type,&v);
    if(ok)*out=(uint32_t)v;
    CFRelease(x);return ok;
}
static io_service_t get_bmd_appledisplay(void){
    CFMutableDictionaryRef dict=IOServiceMatching("AppleDisplay");
    if(!dict)return IO_OBJECT_NULL;
    io_iterator_t it=IO_OBJECT_NULL;
    if(IOServiceGetMatchingServices(kIOMasterPortDefault,dict,&it)!=KERN_SUCCESS)
        return IO_OBJECT_NULL;
    io_service_t obj=IO_OBJECT_NULL,match=IO_OBJECT_NULL;unsigned matches=0;
    while((obj=IOIteratorNext(it))!=IO_OBJECT_NULL){
        uint32_t vendor=0,product=UINT32_MAX;
        if(num(obj,CFSTR("DisplayVendorID"),&vendor) &&
           num(obj,CFSTR("DisplayProductID"),&product) &&
           vendor==BMD_VENDOR && product==BMD_PRODUCT){
            matches++;
            if(!match)match=obj;
            else IOObjectRelease(obj);
        }else IOObjectRelease(obj);
    }
    IOObjectRelease(it);
    if(matches!=1){
        if(match)IOObjectRelease(match);
        return IO_OBJECT_NULL;
    }
    return match;
}
static int param(io_service_t service,CFStringRef key,
                 const char* path,const char* name,uint32_t* out){
    SInt32 v=0,lo=0,hi=0;
    IOReturn rc=IODisplayGetIntegerRangeParameter(service,0,key,&v,&lo,&hi);
    printf("stage=io_display_param path=%s name=%s rc=0x%08x"
           " value=0x%08x min=0x%08x max=0x%08x\n",
           path,name,(unsigned)rc,(unsigned)(uint32_t)v,
           (unsigned)(uint32_t)lo,(unsigned)(uint32_t)hi);
    if(rc)return 0;
    *out=(uint32_t)v;
    return 1;
}
static int colour_pair(io_service_t service,const char* path,
                       uint32_t* selection,uint32_t* control){
    int a=param(service,CFSTR("cmod"),path,"cmod",selection);
    int b=param(service,CFSTR("cyuv"),path,"cyuv",control);
    return a&&b;
}
int main(int argc,char** argv){
    if(argc!=2 || (strcmp(argv[1],"--inspect")&&
                   strcmp(argv[1],"--set422")&&
                   strcmp(argv[1],"--restore444"))){
        puts("usage: --inspect|--set422|--restore444");return 64;
    }
    // IOGraphics-515.3 IOFramebufferParameterHandler::doIntegerSet maps
    // any non-auto cyuv request to RGB, as reproduced on installed 515.4.
    // Permanently block the known-misleading experiment; preserve --restore444.
    if(!strcmp(argv[1],"--set422")){
        puts("stage=color_control result=BLOCKED_IOGRAPHICS_RGB_REWRITE"
             " driver_color_422_unachievable_by_cyuv=1 writes=0"
             " use=inspect_or_restore444");
        return 65;
    }
    int write=strcmp(argv[1],"--inspect")!=0;
    int restore=strcmp(argv[1],"--restore444")==0;
    uint32_t target=restore?COLOR_AUTO:COLOR_422;
    char model[64]={0},osrelease[64]={0};
    size_t ml=sizeof(model),ol=sizeof(osrelease);
    if(sysctlbyname("hw.model",model,&ml,NULL,0) ||
       sysctlbyname("kern.osrelease",osrelease,&ol,NULL,0) ||
       strcmp(model,"Macmini5,3") || strncmp(osrelease,"16.",3)){
        puts("stage=guard result=WRONG_SIERRA_FIXTURE");return 20;
    }
    io_service_t bmd=get_bmd_appledisplay();
    uint32_t reg_cmod=0,reg_cyuv=0,display_cmod=0,display_cyuv=0;
    int registry_read=bmd &&
        colour_pair(bmd,"AppleDisplay",&reg_cmod,&reg_cyuv);

    CGDirectDisplayID ids[16]={0};uint32_t count=0;
    int online=CGGetOnlineDisplayList(16,ids,&count)==kCGErrorSuccess &&
               count==1 && CGDisplayVendorNumber(ids[0])==BMD_VENDOR &&
               CGDisplayModelNumber(ids[0])==BMD_PRODUCT;
    io_service_t quartz=online?CGDisplayIOServicePort(ids[0]):IO_OBJECT_NULL;
    int quartz_read=quartz &&
        colour_pair(quartz,"CGDisplay",&display_cmod,&display_cyuv);
    printf("stage=source_services bmd_apple_display=%d bmd_quartz_online=%d"
           " registry_api_read=%d quartz_api_read=%d\n",
           !!bmd,online,!!registry_read,!!quartz_read);
    if(!registry_read && !quartz_read){
        if(bmd)IOObjectRelease(bmd);return 21;
    }
    uint32_t cmod=quartz_read?display_cmod:reg_cmod;
    uint32_t cyuv=quartz_read?display_cyuv:reg_cyuv;
    io_service_t selected=registry_read?bmd:quartz;
    uint32_t supported=0;
    if(!param(selected,CFSTR("colr"),"active","colr",&supported)){
        if(bmd)IOObjectRelease(bmd);return 22;
    }
    printf("stage=color_selected cmod=0x%08x cyuv=0x%08x"
           " supports_422=%d supports_444=%d\n",
           cmod,cyuv,!!(supported&COLOR_422),!!(supported&COLOR_444));
    if(!write){
        puts("stage=color_control result=READ_ONLY actual_hdmi_wire_encoding=UNVERIFIED");
        if(bmd)IOObjectRelease(bmd);return 0;
    }
    // Restoration is permitted via the unique original AppleDisplay even
    // when mode re-enumeration fails; new colour experiments are not.
    if(!online && !restore){
        puts("stage=color_control result=OFFLINE_CANNOT_START_TRIAL");
        if(bmd)IOObjectRelease(bmd);return 23;
    }
    if(!online && !registry_read){
        puts("stage=color_control result=OFFLINE_NO_RESTORE_SERVICE");
        if(bmd)IOObjectRelease(bmd);return 24;
    }
    if(online){
        CGDisplayModeRef m=CGDisplayCopyDisplayMode(ids[0]);
        if(!m){if(bmd)IOObjectRelease(bmd);return 25;}
        size_t w=CGDisplayModeGetWidth(m),h=CGDisplayModeGetHeight(m);
        double hz=CGDisplayModeGetRefreshRate(m);
        CGDisplayModeRelease(m);
        if(w!=1920 || h!=1080 || hz<29.99 || hz>30.01){
            puts("stage=color_control result=WRONG_FIXED_1080P30_SOURCE_MODE");
            if(bmd)IOObjectRelease(bmd);return 26;
        }
    }
    // The physical cyuv=422 trial selected RGB (cmod=0x1), not 422.
    // Automatic restoration MUST accept RGB as an intermediate state.
    if(cmod!=COLOR_444 && cmod!=COLOR_422 && !(restore && cmod==0x1)){
        puts("stage=color_control result=UNEXPECTED_SELECTED_COLOR");
        if(bmd)IOObjectRelease(bmd);return 27;
    }
    if(!restore && (cyuv!=COLOR_AUTO || cmod!=COLOR_444 ||
                    !(supported&COLOR_422))){
        puts("stage=color_control result=NOT_ORIGINAL_AUTO_SOURCE");
        if(bmd)IOObjectRelease(bmd);return 28;
    }
    if(restore && cyuv==COLOR_AUTO && cmod==COLOR_444){
        puts("stage=color_control result=ALREADY_ORIGINAL_AUTO writes=0");
        if(bmd)IOObjectRelease(bmd);return 0;
    }
    printf("stage=color_write attr=cyuv old_control=0x%08x target_control=0x%08x"
           " old_cmod=0x%08x commit_parameters=0\n",cyuv,target,cmod);
    fflush(stdout);

    io_service_t first=registry_read?bmd:quartz;
    IOReturn rc=IODisplaySetIntegerParameter(first,0,CFSTR("cyuv"),(SInt32)target);
    printf("stage=color_write path=%s rc=0x%08x\n",
           first==bmd?"AppleDisplay":"CGDisplay",(unsigned)rc);
    fflush(stdout);
    if(rc && quartz_read && first!=quartz){
        IOReturn other=IODisplaySetIntegerParameter(
            quartz,0,CFSTR("cyuv"),(SInt32)target);
        printf("stage=color_write fallback_path=CGDisplay rc=0x%08x\n",
               (unsigned)other);
        fflush(stdout);
        rc=other;
    }
    usleep(350000);
    uint32_t cafter=0,ctrl_after=0;
    int readback=colour_pair(first,"after_write",&cafter,&ctrl_after);
    printf("stage=color_write rc=0x%08x result=%s control_after=0x%08x"
           " selected_after=0x%08x commit_called=0 video_streams=0\n",
           (unsigned)rc,rc==0&&readback&&ctrl_after==target&&
           cafter==(restore?COLOR_444:COLOR_422)?"APPLIED":"NOT_VERIFIED",
           ctrl_after,cafter);
    if(bmd)IOObjectRelease(bmd);
    return rc==0&&readback&&ctrl_after==target&&
           cafter==(restore?COLOR_444:COLOR_422)?0:32;
}

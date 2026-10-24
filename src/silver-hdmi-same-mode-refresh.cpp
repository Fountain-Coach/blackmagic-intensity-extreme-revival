// Sierra-native exact HDMI mode preflight/reassert; no alternate source
// Final physical rig: Macmini5,3 HDMI OUT -> original Intensity Extreme IN.
// No pixel capture, no alternate resolutions, no driver or KEXT access.
#include <CoreGraphics/CoreGraphics.h>
#include <sys/sysctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char** argv) {
    const bool reassert=argc==2 && strcmp(argv[1],"--reassert-identical-1080p30")==0;
    const bool check=argc==2 && strcmp(argv[1],"--check")==0;
    if(!reassert && !check) {
        fprintf(stderr,"usage: source-refresh --check|--reassert-identical-1080p30\n");
        return 64;
    }
    char model[64]={0};size_t cap=sizeof(model);
    if(sysctlbyname("hw.model",model,&cap,nullptr,0) ||
       strcmp(model,"Macmini5,3"))return 65;
    CGDirectDisplayID ids[32];uint32_t count=0;
    if(CGGetOnlineDisplayList(32,ids,&count)!=kCGErrorSuccess)return 66;
    int found=0,exitcode=0;
    for(uint32_t i=0;i<count;++i) {
        const CGDirectDisplayID display=ids[i];
        if(CGDisplayVendorNumber(display)!=8961 ||
           CGDisplayPixelsWide(display)!=1920 ||
           CGDisplayPixelsHigh(display)!=1080)continue;
        CGDisplayModeRef original=CGDisplayCopyDisplayMode(display);
        if(!original)continue;
        const double refresh=CGDisplayModeGetRefreshRate(original);
        if(refresh<29.9 || refresh>30.1) {
            CGDisplayModeRelease(original);continue;
        }
        ++found;
        printf("stage=silver_source mode=1920x1080@30 refresh_hz=%.6f "
               "main=%u online=%u active=%u reassert_requested=%u\n",
               refresh,CGDisplayIsMain(display),CGDisplayIsOnline(display),
               CGDisplayIsActive(display),reassert);
        if(reassert) {
            // Requests precisely the same existing display mode, nothing else.
            const CGError result=CGDisplaySetDisplayMode(display,original,nullptr);
            printf("stage=silver_mode_reassert rc=%d identical_mode_only=1\n",
                   (int)result);
            if(result!=kCGErrorSuccess)exitcode=69;
        }
        CGDisplayModeRef after=CGDisplayCopyDisplayMode(display);
        const bool unchanged=after &&
            CGDisplayModeGetWidth(after)==CGDisplayModeGetWidth(original) &&
            CGDisplayModeGetHeight(after)==CGDisplayModeGetHeight(original) &&
            CGDisplayModeGetRefreshRate(after)>29.9 &&
            CGDisplayModeGetRefreshRate(after)<30.1;
        printf("stage=silver_source_verify unchanged=%u source_still_detected=%u "
               "electrical_link_lock_measured=0\n",unchanged,CGDisplayIsOnline(display));
        if(!unchanged)exitcode=70;
        if(after)CGDisplayModeRelease(after);
        CGDisplayModeRelease(original);
    }
    printf("stage=source_preflight exact_silver_displays=%d result=%s\n",
           found,found==1&&!exitcode?"pass":"fail");
    return found==1?exitcode:67;
}

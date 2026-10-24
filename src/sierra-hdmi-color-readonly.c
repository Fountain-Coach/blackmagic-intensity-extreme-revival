/*
 * Original Sierra Macmini5,3: read the driver-selected HDMI colour mode.
 * No display mode/color writes, no DeckLink streams, no screen pixels.
 * Source-owned read-only physical witness for HDMI BMD vendor 0x2301.
 * The old IOGraphics colour constants are:
 *  0x1 RGB; 0x10 YCbCr422; 0x100 YCbCr444;
 *  0x1000 RGB limited; 0x10000000 auto.
 * These are OS-driver selections, not decoded HDMI AVI InfoFrames.
 *
 * clang -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations
 *   src/sierra-hdmi-color-readonly.c -framework CoreGraphics
 *   -framework IOKit -framework CoreFoundation -o /tmp/fc-sierra-hdmi-color-readonly
 */
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char* label(uint32_t v) {
    switch(v) {
        case 0x1: return "RGB";
        case 0x10: return "YCbCr422";
        case 0x100: return "YCbCr444";
        case 0x1000: return "RGB_limited";
        case 0x10000000: return "auto";
        default: return "unknown";
    }
}
static void witness(io_service_t service, CFStringRef key, const char* name) {
    SInt32 value=0,min=0,max=0;
    IOReturn rc=IODisplayGetIntegerRangeParameter(service,0,key,&value,&min,&max);
    printf("stage=io_display_get key=%s rc=0x%08x value=0x%08x min=0x%08x max=0x%08x selected_label=%s\n",
        name,(unsigned)rc,(unsigned)(uint32_t)value,(unsigned)(uint32_t)min,
        (unsigned)(uint32_t)max,rc==0?label((uint32_t)value):"unavailable");
}
int main(void) {
    char model[64]={0},osrelease[64]={0};
    size_t ml=sizeof(model),ol=sizeof(osrelease);
    if(sysctlbyname("hw.model",model,&ml,NULL,0) ||
       sysctlbyname("kern.osrelease",osrelease,&ol,NULL,0) ||
       strcmp(model,"Macmini5,3") || strncmp(osrelease,"16.",3)){
        puts("stage=sierra_fixture result=WRONG_HOST");return 20;
    }
    CGDirectDisplayID ids[16]={0};
    uint32_t n=0;
    CGError err=CGGetOnlineDisplayList(16,ids,&n);
    if(err!=kCGErrorSuccess || n!=1 ||
       CGDisplayVendorNumber(ids[0])!=8961 ||
       CGDisplayModelNumber(ids[0])!=0){
        printf("stage=sierra_fixture result=WRONG_DISPLAY count=%u\n",n);
        return 21;
    }
    CGDisplayModeRef mode=CGDisplayCopyDisplayMode(ids[0]);
    if(!mode){puts("stage=sierra_fixture result=NO_DISPLAY_MODE");return 22;}
    size_t width=CGDisplayModeGetWidth(mode);
    size_t height=CGDisplayModeGetHeight(mode);
    double hz=CGDisplayModeGetRefreshRate(mode);
    CGDisplayModeRelease(mode);
    printf("stage=sierra_fixture result=BMD_HDMI width=%zu height=%zu selected_rate_hz=%.3f\n",
           width,height,hz);
    io_service_t service=CGDisplayIOServicePort(ids[0]);
    if(!service){puts("stage=io_display_get result=no_service");return 23;}
    witness(service,CFSTR("cmod"),"selected_color_mode_cmod");
    witness(service,CFSTR("colr"),"supported_color_modes_colr");
    witness(service,CFSTR("cyuv"),"connection_color_mode_cyuv");
    puts("stage=limits actual_hdmi_wire_color_encoding=not_measured"
         " rgb_framebuffer_is_not_wire_format=1");
    puts("stage=safety display_writes=0 color_writes=0"
         " vendor_streams=0 pci_mmio=0 drivers_firmware_unchanged=1");
    return 0;
}

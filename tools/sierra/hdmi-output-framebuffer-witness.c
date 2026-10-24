/*
 * Fountain Coach — read-only framebuffer evidence for HDMI source display.
 *
 * A Mac can negotiate HDMI/EDID yet never deliver capture-card pixels. This
 * witness hashes two actual OS framebuffer snapshots of the BMD HDMI display.
 * It never opens the DeckLink runtime or claims electrical/video capture.
 *
 * Build on original Sierra:
 * clang -std=c11 -Wall -Wextra -Werror \
 *   tools/sierra/hdmi-output-framebuffer-witness.c \
 *   -framework CoreGraphics -framework CoreFoundation \
 *   -o /tmp/fc-hdmi-output-framebuffer-witness
 *
 * Does not save, print, publish or transmit any desktop pixels.
 */
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CommonCrypto/CommonDigest.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { kBMDDisplayVendor = 8961, kMaxDisplays = 24 };

static int hash_frame(CGDirectDisplayID display, char digest[65],
                      size_t *width, size_t *height, size_t *bytes)
{
    CGImageRef image = CGDisplayCreateImage(display);
    if (!image) return 30;
    CGDataProviderRef provider = CGImageGetDataProvider(image);
    if (!provider) { CGImageRelease(image); return 31; }
    CFDataRef pixels = CGDataProviderCopyData(provider);
    if (!pixels) { CGImageRelease(image); return 32; }
    CFIndex len = CFDataGetLength(pixels);
    if (len <= 0 || (uint64_t)len > UINT32_MAX) {
        CFRelease(pixels);
        CGImageRelease(image);
        return 33;
    }
    const UInt8 *ptr = CFDataGetBytePtr(pixels);
    if (!ptr) { CFRelease(pixels); CGImageRelease(image); return 34; }
    unsigned char hash[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(ptr, (CC_LONG)len, hash);
    for (size_t i = 0; i < CC_SHA256_DIGEST_LENGTH; ++i) {
        (void)snprintf(digest + i * 2, 3, "%02x", hash[i]);
    }
    digest[64] = '\0';
    *width = CGImageGetWidth(image);
    *height = CGImageGetHeight(image);
    *bytes = (size_t)len;
    CFRelease(pixels);
    CGImageRelease(image);
    return 0;
}

int main(void) {
    CGDirectDisplayID displays[kMaxDisplays];
    uint32_t count = 0;
    CGError error = CGGetOnlineDisplayList(kMaxDisplays, displays, &count);
    if (error != kCGErrorSuccess) {
        printf("stage=display_inventory result=failed cg_error=%d\n", error);
        return 20;
    }
    CGDirectDisplayID bmd = kCGNullDirectDisplay;
    for (uint32_t i = 0; i < count; ++i) {
        if (CGDisplayVendorNumber(displays[i]) == kBMDDisplayVendor &&
            CGDisplayIsOnline(displays[i])) {
            bmd = displays[i];
            break;
        }
    }
    if (bmd == kCGNullDirectDisplay) {
        puts("stage=hdmi_output_reference result=not_found vendor=BMD");
        puts("stage=hdmi_capture result=not_tested");
        return 21;
    }
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(bmd);
    if (!mode) {
        puts("stage=hdmi_output_reference result=no_display_mode");
        return 22;
    }
    printf("stage=hdmi_output_display result=found vendor=%u"
           " width=%zu height=%zu rate_hz=%.3f main=%d online=1\n",
           CGDisplayVendorNumber(bmd),
           CGDisplayModeGetWidth(mode), CGDisplayModeGetHeight(mode),
           CGDisplayModeGetRefreshRate(mode),
           (int)CGDisplayIsMain(bmd));
    CGDisplayModeRelease(mode);

    char first[65] = {0}, second[65] = {0};
    size_t w1 = 0, h1 = 0, bytes1 = 0;
    size_t w2 = 0, h2 = 0, bytes2 = 0;
    int rc = hash_frame(bmd, first, &w1, &h1, &bytes1);
    if (rc) {
        printf("stage=hdmi_output_reference result=framebuffer_unavailable"
               " reason=%d\n", rc);
        return rc;
    }
    sleep(1);
    rc = hash_frame(bmd, second, &w2, &h2, &bytes2);
    if (rc) {
        printf("stage=hdmi_output_reference result=second_frame_unavailable"
               " reason=%d\n", rc);
        return rc;
    }
    if (w1 != w2 || h1 != h2 || bytes1 != bytes2) {
        puts("stage=hdmi_output_reference result=framebuffer_geometry_changed");
        return 35;
    }
    printf("stage=hdmi_framebuffer_reference result=observed"
           " width=%zu height=%zu byte_count=%zu\n", w1, h1, bytes1);
    printf("stage=hdmi_framebuffer_change result=%s"
           " interval_s=1.0 source=macos_framebuffer"
           " hash_before=%s hash_after=%s\n",
           strcmp(first, second) == 0 ? "same" : "changed",
           first, second);
    puts("stage=hdmi_receiver_capture result=not_tested"
         " input_lock=unknown received_frame_bytes=unknown");
    puts("stage=safety vendor_driver_calls=0 pci_mmio=0 display_mode_changes=0"
         " screenshots_written=0 network_uploads=0");
    return 0;
}

/*
 * Read-only CoreVideo display-link cadence witness for the Sierra HDMI
 * framebuffer that identifies as BMD HDMI. No screenshot pixels, input
 * capture, DeckLink API, display mode writes, PCI, driver or firmware access.
 *
 * CVDisplayLink output timestamps reflect the macOS display scheduler, not
 * a measured HDMI electrical waveform. Never equate callback cadence to
 * capture-card video input lock.
 *
 * Build: clang -std=c11 -Wall -Wextra -Werror
 *        tools/sierra/hdmi-displaylink-clock-witness.c
 *        -framework CoreGraphics -framework CoreVideo
 *        -o /tmp/fc-hdmi-displaylink-clock
 *
 * Copyright (c) 2026 Fountain Coach contributors. MIT.
 */
#include <CoreGraphics/CoreGraphics.h>
#include <CoreVideo/CoreVideo.h>
#include <mach/mach_time.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <unistd.h>

enum { kMaxSamples = 120, kRequiredSamples = 85, kBMDVendor = 8961 };
struct Witness {
    atomic_uint count;
    uint64_t host_times[kMaxSamples];
};
static CVReturn on_display(
    CVDisplayLinkRef link, const CVTimeStamp *now,
    const CVTimeStamp *output, CVOptionFlags flags_in,
    CVOptionFlags *flags_out, void *ctx)
{
    (void)link; (void)now; (void)flags_in; (void)flags_out;
    struct Witness *w = (struct Witness*)ctx;
    unsigned i = atomic_fetch_add_explicit(&w->count, 1, memory_order_relaxed);
    if (i < kMaxSamples) {
        w->host_times[i] = output->hostTime;
    }
    return kCVReturnSuccess;
}
static int compare_doubles(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return (x > y) - (x < y);
}
static double hz_cvtime(CVTime value) {
    if (value.timeValue <= 0 || value.timeScale <= 0) return 0.0;
    return (double)value.timeScale / (double)value.timeValue;
}
int main(void) {
    char model[64] = {0};
    size_t len = sizeof(model);
    if (sysctlbyname("hw.model", model, &len, NULL, 0) != 0 ||
        strcmp(model, "Macmini5,3") != 0) {
        puts("stage=displaylink result=unexpected_host"); return 20;
    }
    CGDirectDisplayID ids[16] = {0};
    uint32_t n = 0;
    if (CGGetOnlineDisplayList(16, ids, &n) != kCGErrorSuccess || n != 1 ||
        CGDisplayVendorNumber(ids[0]) != kBMDVendor ||
        CGDisplayModelNumber(ids[0]) != 0 ||
        CGDisplayPixelsWide(ids[0]) != 1920 ||
        CGDisplayPixelsHigh(ids[0]) != 1080) {
        printf("stage=displaylink result=unexpected_display online=%u\n", n);
        return 21;
    }
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(ids[0]);
    if (!mode) return 22;
    double ui_hz = CGDisplayModeGetRefreshRate(mode);
    CGDisplayModeRelease(mode);
    if (ui_hz < 29.99 || ui_hz > 30.01) {
        printf("stage=displaylink result=unexpected_ui_rate hz=%.6f\n", ui_hz);
        return 23;
    }
    CVDisplayLinkRef link = NULL;
    CVReturn rc = CVDisplayLinkCreateWithCGDisplay(ids[0], &link);
    if (rc != kCVReturnSuccess || !link) {
        printf("stage=displaylink result=creation_failed rc=%d\n", (int)rc);
        return 24;
    }
    struct Witness w;
    memset(&w, 0, sizeof(w));
    atomic_init(&w.count, 0);
    rc = CVDisplayLinkSetOutputCallback(link, on_display, &w);
    if (rc != kCVReturnSuccess) {
        printf("stage=displaylink result=callback_registration_failed rc=%d\n", (int)rc);
        CVDisplayLinkRelease(link); return 25;
    }
    CVTime nominal = CVDisplayLinkGetNominalOutputVideoRefreshPeriod(link);
    rc = CVDisplayLinkStart(link);
    if (rc != kCVReturnSuccess) {
        printf("stage=displaylink result=start_failed rc=%d\n", (int)rc);
        CVDisplayLinkRelease(link); return 26;
    }
    // Fixed upper bound: 800 * 10ms = eight seconds of timestamp-only sampling.
    for (unsigned i = 0; i < 800; ++i) {
        if (atomic_load_explicit(&w.count, memory_order_relaxed) >= kMaxSamples)
            break;
        usleep(10000);
    }
    double actual_period_seconds = CVDisplayLinkGetActualOutputVideoRefreshPeriod(link);
    CVDisplayLinkStop(link);
    unsigned samples = atomic_load_explicit(&w.count, memory_order_relaxed);
    if (samples > kMaxSamples) samples = kMaxSamples;
    CVDisplayLinkRelease(link);
    printf("stage=displaylink source=BMD_HDMI ui_rate_hz=%.6f nominal_link_hz=%.6f actual_link_hz=%.6f callbacks=%u\n",
           ui_hz, hz_cvtime(nominal),\n           actual_period_seconds > 0.0 ? 1.0 / actual_period_seconds : 0.0, samples);
    if (samples < kRequiredSamples) {
        puts("stage=displaylink result=insufficient_callback_samples");
        return 27;
    }
    mach_timebase_info_data_t timebase = {0, 0};
    if (mach_timebase_info(&timebase) != KERN_SUCCESS ||
        !timebase.denom || !timebase.numer) return 28;
    double periods[kMaxSamples] = {0.0};
    unsigned count = 0;
    for (unsigned i = 1; i < samples; ++i) {
        if (!w.host_times[i] || w.host_times[i] <= w.host_times[i-1]) continue;
        uint64_t ticks = w.host_times[i] - w.host_times[i-1];
        double seconds = (double)ticks * (double)timebase.numer /
                         (double)timebase.denom * 1e-9;
        if (seconds > 0.001 && seconds < 0.150) periods[count++] = seconds;
    }
    if (count < kRequiredSamples - 1) {
        puts("stage=displaylink result=insufficient_timestamps");
        return 29;
    }
    qsort(periods, count, sizeof(periods[0]), compare_doubles);
    double median = periods[count / 2];
    // Trim 10% from each side to reduce callback scheduling outliers.
    unsigned trim = count / 10;
    double total = 0.0;
    for (unsigned i = trim; i < count - trim; ++i) total += periods[i];
    double mean = total / (double)(count - 2 * trim);
    printf("stage=displaylink_cadence result=observed intervals=%u median_hz=%.6f trimmed_mean_hz=%.6f median_period_ms=%.6f\n",
           count, 1.0 / median, 1.0 / mean, 1000.0 * median);
    puts("stage=limits displaylink_callback_timestamps_not_hdmi_wire_clock=1 actual_wire_encoding=unknown receiver_signal_lock=not_tested");
    puts("stage=safety monitor_mode_changes=0 decklink_calls=0 pci_mmio=0 frame_pixels=0 firmware_driver_changes=0");
    return 0;
}

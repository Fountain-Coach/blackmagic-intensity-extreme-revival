/*
 * Fountain Coach - Legacy Syphon server witness (Sierra only).
 *
 * A standalone OPTIONAL diagnostic for comparing the historical VIDVOX
 * Black Syphon capture bridge with our own DeckLink input implementation.
 * Does NOT embed Syphon, open the capture device, or record any pixels.
 *
 * Copyright (c) 2026 Fountain Coach contributors. MIT.
 */
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <dlfcn.h>
#include <stdio.h>

static id fc_message0(id receiver, SEL selector) {
    return ((id(*)(id,SEL))objc_msgSend)(receiver, selector);
}

int main(void) {
    @autoreleasepool {
        const char *path =
            "/Applications/Black Syphon.app/Contents/Frameworks/"
            "Syphon.framework/Syphon";
        void *syphon = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!syphon) {
            puts("stage=legacy_syphon result=framework_unavailable");
            return 20;
        }
        Class cls = objc_getClass("SyphonServerDirectory");
        if (!cls) {
            puts("stage=legacy_syphon result=directory_unavailable");
            return 21;
        }
        id directory = fc_message0((id)cls, sel_registerName("sharedDirectory"));
        [[NSRunLoop currentRunLoop]
            runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.3]];
        id servers = fc_message0(directory, sel_registerName("servers"));
        if (![servers isKindOfClass:[NSArray class]]) {
            puts("stage=legacy_syphon result=invalid_directory");
            return 22;
        }
        NSString * const *nameKey = (NSString * const *)
            dlsym(syphon, "SyphonServerDescriptionNameKey");
        NSString * const *appKey = (NSString * const *)
            dlsym(syphon, "SyphonServerDescriptionAppNameKey");
        for (id entry in (NSArray*)servers) {
            if (![entry isKindOfClass:[NSDictionary class]]) continue;
            NSString *name = nameKey ?
                [entry objectForKey:*nameKey] : nil;
            NSString *app = appKey ?
                [entry objectForKey:*appKey] : nil;
            if (!name) name = [entry objectForKey:@"name"];
            if (!app) app = [entry objectForKey:@"appName"];
            printf("stage=legacy_syphon_server app=%s source=%s\n",
                app ? [app UTF8String] : "unknown",
                name ? [name UTF8String] : "unknown");
        }
        printf("stage=legacy_syphon result=enumerated server_count=%lu"
               " frame_pixels_verified=0 hardware_control=0\n",
               (unsigned long)[(NSArray*)servers count]);
        return 0;
    }
}

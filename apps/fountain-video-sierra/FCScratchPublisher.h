/*
 * Fountain Coach: native Sierra -> private WLAN browser frame transport.
 *
 * The app's renderer is the only video producer. Both generated test video
 * and observed DeckLink frames use this SAME bounded JPEG publication path.
 * No Syphon, CoreMediaIO, device access, kernel change or server-code install.
 */
#import <Cocoa/Cocoa.h>

@interface FCScratchPublisher : NSObject

@property(nonatomic,assign) BOOL enabled;
@property(nonatomic,readonly) NSUInteger delivered;
@property(nonatomic,readonly) NSUInteger failed;

- (instancetype)initWithEndpoint:(NSURL *)endpoint token:(NSString *)token;
- (void)publishImage:(NSImage *)image ptsNs:(int64_t)ptsNs
              origin:(NSString *)origin;

// Deterministic image-to-JPEG conversion is independently self-testable
// without a receiver, network, Nikon or DeckLink runtime.
+ (NSData *)jpegPreviewForImage:(NSImage *)image
                     outWidth:(NSUInteger *)outWidth
                    outHeight:(NSUInteger *)outHeight;

@end

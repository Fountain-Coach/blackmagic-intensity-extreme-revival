#import "FCScratchPublisher.h"

@implementation FCScratchPublisher {
    NSURL *_endpoint;
    NSString *_token;
    NSURLSession *_session;
    NSTimeInterval _lastSend;
    NSUInteger _outstanding;
    NSUInteger _delivered;
    NSUInteger _failed;
}

@synthesize enabled = _enabled;

- (instancetype)initWithEndpoint:(NSURL *)endpoint token:(NSString *)token {
    self = [super init];
    if (!self) return nil;
    if (!endpoint || ![endpoint.scheme.lowercaseString isEqualToString:@"http"] ||
        token.length < 32 || ![endpoint.path isEqualToString:@"/api/scratch/ingest"]) {
        NSLog(@"Fountain Scratch: invalid trusted LAN endpoint or ingest token");
        return nil;
    }
    _endpoint = [endpoint copy];
    _token = [token copy];
    _enabled = YES;
    _lastSend = -1;
    NSURLSessionConfiguration *config =
        [NSURLSessionConfiguration ephemeralSessionConfiguration];
    config.timeoutIntervalForRequest = 4;
    config.timeoutIntervalForResource = 4;
    config.HTTPShouldSetCookies = NO;
    config.URLCache = nil;
    _session = [NSURLSession sessionWithConfiguration:config];
    return self;
}

- (NSUInteger)delivered { return _delivered; }
- (NSUInteger)failed { return _failed; }

+ (NSData *)jpegPreviewForImage:(NSImage *)image
                     outWidth:(NSUInteger *)outWidth
                    outHeight:(NSUInteger *)outHeight {
    if (!image || image.size.width < 1 || image.size.height < 1) return nil;
    const double fraction = MIN(1.0, MIN(640.0/image.size.width,
                                         360.0/image.size.height));
    const NSUInteger width=(NSUInteger)MAX(1,lrint(image.size.width*fraction));
    const NSUInteger height=(NSUInteger)MAX(1,lrint(image.size.height*fraction));
    if (width>640 || height>360 || width<1 || height<1) return nil;
    NSBitmapImageRep *rep=[[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:NULL
        pixelsWide:(NSInteger)width pixelsHigh:(NSInteger)height
        bitsPerSample:8 samplesPerPixel:3 hasAlpha:NO isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:(NSInteger)width*3
        bitsPerPixel:24];
    if (!rep) return nil;
    NSGraphicsContext *graphics=[NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
    if (!graphics) return nil;
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:graphics];
    [[NSColor blackColor] setFill];
    NSRectFill(NSMakeRect(0,0,width,height));
    [image drawInRect:NSMakeRect(0,0,width,height)
            fromRect:NSZeroRect operation:NSCompositingOperationSourceOver
            fraction:1 respectFlipped:NO hints:nil];
    [graphics flushGraphics];
    [NSGraphicsContext restoreGraphicsState];
    NSData *jpeg=[rep representationUsingType:NSJPEGFileType properties:@{
        NSImageCompressionFactor:@0.65
    }];
    if (!jpeg || jpeg.length<4 || jpeg.length>1048576) return nil;
    const uint8_t *p=(const uint8_t*)jpeg.bytes;
    if (p[0]!=0xff || p[1]!=0xd8 ||
        p[jpeg.length-2]!=0xff || p[jpeg.length-1]!=0xd9) return nil;
    if (outWidth) *outWidth=width;
    if (outHeight) *outHeight=height;
    return jpeg;
}

- (void)publishImage:(NSImage *)image ptsNs:(int64_t)ptsNs
              origin:(NSString *)origin {
    if (!_enabled || !_session || !image || ptsNs < 0 ||
        (![origin isEqualToString:@"synthetic"] &&
         ![origin isEqualToString:@"decklink-observed"])) return;
    // This method is called from the AppKit main thread. Do not accumulate
    // background uploads if WLAN delivery becomes slow.
    NSTimeInterval now=[NSDate timeIntervalSinceReferenceDate];
    if (_outstanding>=2 || (_lastSend>=0 && now-_lastSend<0.17)) return;
    _lastSend=now;
    NSUInteger width=0,height=0;
    NSData *jpeg=[FCScratchPublisher jpegPreviewForImage:image
                                               outWidth:&width outHeight:&height];
    if (!jpeg) { ++_failed; return; }
    NSMutableURLRequest *request=[NSMutableURLRequest requestWithURL:_endpoint];
    request.HTTPMethod=@"POST";
    request.HTTPBody=jpeg;
    [request setValue:@"image/jpeg" forHTTPHeaderField:@"Content-Type"];
    [request setValue:[@"Bearer " stringByAppendingString:_token]
      forHTTPHeaderField:@"Authorization"];
    [request setValue:[NSString stringWithFormat:@"%lld",(long long)ptsNs]
      forHTTPHeaderField:@"X-Fountain-PTS-Ns"];
    [request setValue:origin forHTTPHeaderField:@"X-Fountain-Origin"];
    [request setValue:[NSString stringWithFormat:@"%lu",(unsigned long)width]
      forHTTPHeaderField:@"X-Fountain-Width"];
    [request setValue:[NSString stringWithFormat:@"%lu",(unsigned long)height]
      forHTTPHeaderField:@"X-Fountain-Height"];
    ++_outstanding;
    __weak FCScratchPublisher *weakSelf=self;
    NSURLSessionDataTask *task=[_session dataTaskWithRequest:request
        completionHandler:^(NSData *responseData, NSURLResponse *response, NSError *error) {
        NSHTTPURLResponse *http=(NSHTTPURLResponse*)response;
        BOOL ok=!error && [http isKindOfClass:[NSHTTPURLResponse class]] &&
                http.statusCode==201;
        dispatch_async(dispatch_get_main_queue(), ^{
            FCScratchPublisher *strong=weakSelf;
            if (!strong) return;
            if (strong->_outstanding>0) --strong->_outstanding;
            if (ok) ++strong->_delivered;
            else {
                ++strong->_failed;
                if (strong->_failed<=2 || strong->_failed%60==0) {
                    NSLog(@"Fountain Scratch frame publish failed (%lu): %@ HTTP %ld",
                          (unsigned long)strong->_failed,
                          error.localizedDescription,
                          (long)http.statusCode);
                }
            }
        });
    }];
    [task resume];
}

- (void)dealloc {
    [_session invalidateAndCancel];
}

@end

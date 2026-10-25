/*
 * Fountain Coach — macOS Sierra LaunchServices Panasonic video performance.
 *
 * Ordinary user-owned AppKit application, no persistent LaunchAgent, no
 * launchctl bootstrap, no shell-owned detached process, no GUI automation,
 * and no security changes. Launch via "open -a".
 *
 * The app supervises the source-owned DeckLink output adapter for a bounded
 * 20-minute performance. Exit of the hardware adapter terminates this app.
 * A normal macOS app session, not SSH, owns the performance lifetime.
 *
 * The adapter itself verifies the exact original Sierra host, physical
 * Intensity Extreme, device idle and selected HDMI output. No device input
 * is opened, no driver replaced, no firmware or HDMI mode of the Mac changed.
 *
 * Copyright (c) Fountain Coach contributors, 2026. MIT.
 */
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static volatile sig_atomic_t gTerminate=0;
static void request_stop(int sig){(void)sig;gTerminate=1;}

@interface FountainPanasonicApp : NSObject <NSApplicationDelegate>
@property (strong) NSTask *output;
@property (strong) NSString *runDirectory;
@property (strong) NSFileHandle *logFile;
@property (strong) NSTimer *monitor;
@end

@implementation FountainPanasonicApp
- (void)log:(NSString *)line {
    NSString *msg=[NSString stringWithFormat:
        @"stage=panasonic_app %@\n",line];
    NSData *data=[msg dataUsingEncoding:NSUTF8StringEncoding];
    if(self.logFile)[self.logFile writeData:data];
}
- (void)applicationDidFinishLaunching:(NSNotification *)note {
    (void)note;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
    NSString *base=[NSHomeDirectory() stringByAppendingPathComponent:
       @"Library/Application Support/Fountain Coach/Video Instrument"];
    self.runDirectory=[base stringByAppendingPathComponent:@"run"];
    NSError *error=nil;
    if(![[NSFileManager defaultManager] createDirectoryAtPath:self.runDirectory
       withIntermediateDirectories:YES attributes:nil error:&error]){
        NSLog(@"Panasonic show: cannot create log directory: %@",error);
        [NSApp terminate:nil];return;
    }
    NSString *logPath=[self.runDirectory
        stringByAppendingPathComponent:@"panasonic-gui-performance.log"];
    if(![[NSFileManager defaultManager] fileExistsAtPath:logPath]){
        [[NSFileManager defaultManager] createFileAtPath:logPath
                   contents:[NSData data] attributes:nil];
    }
    self.logFile=[NSFileHandle fileHandleForWritingAtPath:logPath];
    if(!self.logFile){[NSApp terminate:nil];return;}
    [self.logFile seekToEndOfFile];
    NSString *binary=[base stringByAppendingPathComponent:
                       @"bin/fountain-video-output"];
    if(![[NSFileManager defaultManager] isExecutableFileAtPath:binary]){
        [self log:@"result=NO_INSTALLED_HARDWARE_OUTPUT"];
        [NSApp terminate:nil];return;
    }
    self.output=[[NSTask alloc] init];
    [self.output setLaunchPath:binary];
    [self.output setArguments:@[@"--mode",@"720p60",@"--minutes",@"20"]];
    /* Share one append-only log, without exposing anything over a network. */
    [self.output setStandardOutput:self.logFile];
    [self.output setStandardError:self.logFile];
    [[NSNotificationCenter defaultCenter] addObserver:self
        selector:@selector(outputFinished:)
        name:NSTaskDidTerminateNotification object:self.output];
    [self log:@"starting=1 mode=720p60 lease_minutes=20 appkit_owner=1"];
    @try {
        [self.output launch];
    } @catch (NSException *e) {
        [self log:[NSString stringWithFormat:
                    @"result=LAUNCH_FAILED exception=%@",[e name]]];
        [NSApp terminate:nil];return;
    }
    [self log:[NSString stringWithFormat:
               @"result=OUTPUT_STARTED pid=%d", [self.output processIdentifier]]];
    self.monitor=[NSTimer scheduledTimerWithTimeInterval:0.5 target:self
           selector:@selector(checkStop:) userInfo:nil repeats:YES];
}
- (void)checkStop:(NSTimer *)timer {
    (void)timer;
    if(gTerminate)[NSApp terminate:nil];
}
- (void)outputFinished:(NSNotification *)note {
    (void)note;
    int status=[self.output terminationStatus];
    [self log:[NSString stringWithFormat:@"result=OUTPUT_EXITED code=%d",status]];
    [NSApp terminate:nil];
}
- (void)applicationWillTerminate:(NSNotification *)note {
    (void)note;
    [self.monitor invalidate];self.monitor=nil;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if([self.output isRunning]){
        [self log:@"result=STOP_REQUESTED_TO_OUTPUT"];
        [self.output terminate];
    }
    [self log:@"result=APP_EXITING"];
    [self.logFile closeFile];
    self.logFile=nil;
}
@end

int main(void){
    @autoreleasepool{
        signal(SIGTERM,request_stop);
        signal(SIGINT,request_stop);
        NSApplication *app=[NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        FountainPanasonicApp *delegate=[[FountainPanasonicApp alloc] init];
        [app setDelegate:delegate];
        [app run];
        return 0;
    }
}

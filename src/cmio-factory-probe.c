// Read-only, isolated CoreFoundation CFPlugIn factory instantiation probe.
// Do not use as system plugin registration; never writes system preferences.
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
int main(int argc,char **argv){
 if(argc!=2)return 64;
 CFStringRef path=CFStringCreateWithCString(NULL,argv[1],kCFStringEncodingUTF8);
 CFURLRef url=CFURLCreateWithFileSystemPath(NULL,path,kCFURLPOSIXPathStyle,true);
 CFRelease(path);
 if(!url)return 20;
 CFPlugInRef plugin=CFPlugInCreate(NULL,url);
 CFRelease(url);
 printf("phase=create_plugin status=%s\n",plugin?"ok":"failed");fflush(stdout);
 if(!plugin)return 21;
 CFUUIDRef factory=CFUUIDCreateFromString(NULL,CFSTR("4675C20F-0269-4425-9F5D-E6C6C6CF4A3B"));
 CFUUIDRef type=CFUUIDCreateFromString(NULL,CFSTR("30010C1C-93BF-11D8-8B5B-000A95AF9C6A"));
 if(!factory||!type)return 22;
 printf("phase=create_instance starting\n");fflush(stdout);
 void* inst=CFPlugInInstanceCreate(NULL,factory,type);
 printf("phase=create_instance status=%s\n",inst?"ok":"failed");fflush(stdout);
 CFRelease(factory);CFRelease(type);
 // The legacy CMIO object is COM-like; do not assume it is a CFTypeRef.
 // The instance ends with this short-lived diagnostic process.
 CFRelease(plugin);
 return inst?0:23;
}

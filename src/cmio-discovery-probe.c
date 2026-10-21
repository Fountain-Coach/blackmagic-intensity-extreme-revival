#include <CoreMediaIO/CMIOHardware.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
  CMIOObjectPropertyAddress address={kCMIOHardwarePropertyDevices,kCMIOObjectPropertyScopeGlobal,kCMIOObjectPropertyElementMaster};
  UInt32 size=0, used=0;
  puts("step=read_devices_size"); fflush(stdout);
  OSStatus rc=CMIOObjectGetPropertyDataSize(kCMIOObjectSystemObject,&address,0,NULL,&size);
  printf("size_status=%d bytes=%u\n",(int)rc,(unsigned)size);fflush(stdout);
  if(rc||size>4096||size%sizeof(CMIODeviceID))return 20;
  CMIODeviceID* devices=(CMIODeviceID*)calloc(size?size:1,1);
  puts("step=read_devices");fflush(stdout);
  rc=CMIOObjectGetPropertyData(kCMIOObjectSystemObject,&address,0,NULL,size,&used,devices);
  printf("devices_status=%d bytes=%u count=%u\n",(int)rc,(unsigned)used,(unsigned)(used/sizeof(CMIODeviceID)));fflush(stdout);
  if(!rc)for(UInt32 i=0;i<used/sizeof(CMIODeviceID);++i)printf("device_id=%u\n",devices[i]);
  free(devices);
  return rc?21:0;
}

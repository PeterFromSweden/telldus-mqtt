#ifndef TELLDUSDEVICE_H_
#define TELLDUSDEVICE_H_

typedef enum {
  TM_DEVICE_CONTENT_NONE,
  TM_DEVICE_CONTENT_DEVICENO,
} TDeviceContent;

typedef struct {
  int device_number; // = device_no as int
  char device_no[5]; // = device_number as string
  char command_topic[100];
  char state_topic[100];
  char value[10];
} TelldusDevice;

TelldusDevice* TelldusDevice_Create(int deviceNo);
TelldusDevice* TelldusDevice_GetTopic(const char* topic);
char* TelldusDevice_ToString(TelldusDevice* self, char* strp, int len);
char* TelldusDevice_ItemToString(TelldusDevice* self, TDeviceContent content);
void TelldusDevice_Action(TelldusDevice* self, const char* action);

// Callbacks are static methods with context as telldusclient instance reference
void TelldusDevice_OnEvent(int deviceId, int method, const char *data, int callbackId, void *context);

#endif // TELLDUSDEVICE_H_
#ifndef TELLDUSCLIENT_H_
#define TELLDUSCLIENT_H_

#include <stdbool.h>

typedef enum {
  TM_DEVICE_GET_FIRST,
  TM_DEVICE_GET_NEXT,
} TDeviceGetOp;

typedef struct
{
  int controllerId;
  char controllerSerial[20];
  int evtController;
  int evtDeviceChange;
  int evtDevice;
  int evtSensor;
  int evtRawDevice;
  int deviceCount;
  int lastDeviceIx;
  bool mutelog;
  bool lograw;
  bool disconnectRequest;
} TelldusClient;

TelldusClient* TelldusClient_GetInstance(void);
void TelldusClient_Destroy(TelldusClient *self);
void TelldusClient_SetLogRaw(TelldusClient *self, bool lograw);
bool TelldusClient_IsConnected(TelldusClient *self);
int TelldusClient_Connect(TelldusClient *self);
void TelldusClient_Disconnect(TelldusClient *self);
int TelldusClient_GetDeviceNo(TelldusClient *self, TDeviceGetOp op);
static inline char* TelldusClient_GetControllerSerial(TelldusClient *self)
{
  return self->controllerSerial;
}

#endif // TELLDUSCLIENT_H_
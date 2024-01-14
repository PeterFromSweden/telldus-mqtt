#ifndef TELLDUSCLIENT_H_
#define TELLDUSCLIENT_H_

#include <stdbool.h>

typedef struct
{
  int controllerId;
  char controllerSerial[20];
  int evtController;
  int evtDeviceChange;
  int evtDevice;
  int evtSensor;
  int evtRawDevice;
  bool mutelog;
  bool disconnectRequest;
} TelldusClient;

TelldusClient* TelldusClient_GetInstance(void);
bool TelldusClient_IsConnected(TelldusClient *self);
int TelldusClient_Connect(TelldusClient *self);
void TelldusClient_Disconnect(TelldusClient *self);
static inline char* TelldusClient_GetControllerSerial(TelldusClient *self)
{
  return self->controllerSerial;
}

#endif // TELLDUSCLIENT_H_
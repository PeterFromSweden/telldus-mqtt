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

int TelldusClient_Init(TelldusClient *self);
bool TelldusClient_IsConnected(TelldusClient *self);
int TelldusClient_Connect(TelldusClient *self);
void TelldusClient_Disconnect(TelldusClient *self);

#endif // TELLDUSCLIENT_H_
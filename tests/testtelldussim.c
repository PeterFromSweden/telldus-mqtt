// Tests of the telldus side of telldus-mqtt against the TellStick simulator.
// No MQTT broker is needed.
#include <stdio.h>
#include <string.h>
#include <telldus-core.h>
#include "telldussim/telldussim.h"
#include "telldusclient.h"
#include "telldusdevice.h"
#include "mythread.h"
#include "log.h"

#define CHECK(c) if(!(c)) { printf("FAIL %s:%i: %s\n", __FILE__, __LINE__, #c); return 1; }

static int testNoTellStick(void)
{
  TelldusSim_Reset();
  TelldusClient* client = TelldusClient_GetInstance();
  CHECK( TelldusClient_Connect(client) == -1 );
  CHECK( !TelldusClient_IsConnected(client) );
  return 0;
}

static int testConnectAndUnplug(void)
{
  TelldusSim_Reset();
  TelldusSim_SetController(5, TELLSTICK_CONTROLLER_TELLSTICK_DUO, "SIMSERIAL", true);
  TelldusSim_AddDevice(1);

  TelldusClient* client = TelldusClient_GetInstance();
  CHECK( TelldusClient_Connect(client) == 0 );
  CHECK( TelldusClient_IsConnected(client) );
  CHECK( strcmp(TelldusClient_GetControllerSerial(client), "SIMSERIAL") == 0 );

  TelldusSim_UnplugController();
  CHECK( !TelldusClient_IsConnected(client) );
  return 0;
}

static int testDeviceActionIsRepeatedOnce(void)
{
  TelldusSim_Reset();
  TelldusSim_SetController(5, TELLSTICK_CONTROLLER_TELLSTICK_DUO, "SIMSERIAL", true);
  TelldusSim_AddDevice(1);
  TelldusClient* client = TelldusClient_GetInstance();
  CHECK( TelldusClient_Connect(client) == 0 );

  TelldusDevice* device = TelldusDevice_Create(1);
  CHECK( device != NULL );
  CHECK( TelldusDevice_Create(1) == device );

  TelldusDevice_Action(device, "ON");
  CHECK( TelldusSim_GetTurnOnCount(1) == 1 );
  MyThread_Sleep(1500);
  CHECK( TelldusSim_GetTurnOnCount(1) == 2 ); // RF resend after 1s
  MyThread_Sleep(1500);
  CHECK( TelldusSim_GetTurnOnCount(1) == 2 ); // ...and only once

  TelldusDevice_Action(device, "off");
  CHECK( TelldusSim_GetTurnOffCount(1) == 1 );
  MyThread_Sleep(1500);
  CHECK( TelldusSim_GetTurnOffCount(1) == 2 );

  // Payloads come from the network: must not overflow lastAction
  TelldusDevice_Action(device, "a-payload-much-longer-than-the-action-buffer");
  TelldusDevice_Action(device, NULL);
  CHECK( TelldusSim_GetTurnOnCount(1) == 2 );
  CHECK( TelldusSim_GetTurnOffCount(1) == 2 );

  TelldusClient_Disconnect(client);
  return 0;
}

int main(void)
{
  Log_Init(TM_LOG_CONSOLE, "testtelldussim", TM_LOG_DEBUG, false);
  int res = 0;
  res |= testNoTellStick();
  res |= testConnectAndUnplug();
  res |= testDeviceActionIsRepeatedOnce();
  printf("%s\n", res ? "FAILED" : "PASSED");
  return res;
}

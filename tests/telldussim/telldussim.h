#ifndef TELLDUSSIM_H_
#define TELLDUSSIM_H_

// TellStick simulator: a stand-in for libtelldus-core used by the tests.
// It implements the td* functions telldus-mqtt calls and lets a test
// play the role of telldusd and the RF world (controllers, devices,
// sensor readings).
//
// Events are delivered synchronously on the calling thread.

#include <stdbool.h>

#define TELLDUSSIM_MAX_DEVICES 16

void TelldusSim_Reset(void);

// Plug in a controller. Only one controller is simulated.
void TelldusSim_SetController(int id, int type, const char* serial, bool available);
// Unplug the controller and notify like telldusd does (changeEvent 4).
void TelldusSim_UnplugController(void);

// Add a configured device (as in tellstick.conf).
void TelldusSim_AddDevice(int deviceId);

// Simulate a received RF sensor reading.
void TelldusSim_SensorEvent(const char* protocol, const char* model, int id, int dataType, const char* value);
// Simulate a received RF device event (e.g. a remote control button).
void TelldusSim_DeviceEvent(int deviceId, int method);

// What telldus-mqtt has asked the simulator to send.
int TelldusSim_GetTurnOnCount(int deviceId);
int TelldusSim_GetTurnOffCount(int deviceId);

#endif // TELLDUSSIM_H_

#ifndef STATUS_H_
#define STATUS_H_

#include <stdbool.h>

// Status file: a JSON file with the sensors and switches heard, their last
// values and the connection state. Other programs (e.g. LuCI) read it.
// All functions do nothing until Status_Init() is called with a path.
// All functions are thread safe, except Status_Init() and Status_Destroy().
// Do not call them from a signal handler (MyTimer callbacks on Linux).

#define STATUS_FORMAT       1
#define STATUS_MAX_DEVICES  64  // Sensors and switches
#define STATUS_MAX_VALUES   4   // Datatypes per sensor
#define STATUS_MAX_EVENTS   20  // Raw RF command events
#define STATUS_EVENT_LEN    160 // Raw event text, longer is cut

// Enable the status file. Call after the config is loaded. Returns 0 on success.
int Status_Init(const char* path);
void Status_Destroy(void);
bool Status_IsEnabled(void);

// Connection state
void Status_SetService(bool connected);
void Status_SetGateway(bool found, const char* type, const char* serial);
void Status_SetBroker(bool connected);

// Devices. dataType is the name used in the MQTT topic, e.g. "Temperature".
void Status_SensorValue(const char* protocol, const char* model, int id, const char* dataType, const char* value);
void Status_AddSwitch(int deviceNumber);
void Status_SwitchValue(int deviceNumber, const char* state);

// A raw RF message was received, e.g. "class:command;protocol:arctech;...;".
// Command events are listed, newest first. Sensor events only set last_rf.
void Status_RawEvent(const char* data);

// Write the file if something changed, at most one time per second.
// Call this about one time per second.
void Status_Poll(void);
// Write the file now. Returns 0 on success.
int Status_Write(void);
// The program stops: write the file a last time, with "running": false.
void Status_Stop(void);

#endif // STATUS_H_

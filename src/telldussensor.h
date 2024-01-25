#ifndef TELLDUSSENSOR_H_
#define TELLDUSSENSOR_H_

#include "mytimer.h"

typedef enum {
  TM_SENSOR_CONTENT_NONE,
  TM_SENSOR_CONTENT_DATATYPE,
  TM_SENSOR_CONTENT_UNIT,
  TM_SENSOR_CONTENT_PROTOCOL,
  TM_SENSOR_CONTENT_MODEL,
  TM_SENSOR_CONTENT_ID,
} TSensorContent;

typedef struct {
  char protocol[30];
  char model[30];
  char id[10];
  char dataType[20];
  char unit[20];
  char state_topic[100];
  char availability[100];
  // Dynamic part
  MyTimer* myTimer;
  char value[10];
  int timestamp;
  bool online;
} TelldusSensor;

// Callbacks are static methods with context as telldusclient instance reference
void TelldusSensor_OnEvent(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp, int callbackId, void *context);

char* TelldusSensor_ToString(TelldusSensor* self, char* strp, int len);
char* TelldusSensor_ItemToString(TelldusSensor* self, TSensorContent content);
#endif // TELLDUSSENSOR_H_
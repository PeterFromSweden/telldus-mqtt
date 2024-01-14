#ifndef TELLDUSSENSOR_H_
#define TELLDUSSENSOR_H_

typedef struct {
  char protocol[30];
  char model[30];
  int id;
  int dataType;
} TelldusSensor;

void TelldusSensor_OnEvent(const char *protocol, const char *model, int id, int dataType, const char *value, int timestamp, int callbackId, void *context);

#endif // TELLDUSSENSOR_H_
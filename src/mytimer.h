#ifndef MYTIMER_H_
#define MYTIMER_H_

struct myTimer;
typedef struct myTimer MyTimer;

typedef void (*MyTimerCallback)(MyTimer* myTimer);

MyTimer* MyTimer_Create(MyTimerCallback callback, void* callbackData);
void MyTimer_Destroy(MyTimer* self);
void MyTimer_Start(MyTimer* self, int timeout_ms);
void* MyTimer_GetCallbackData(MyTimer* self);


#endif // MYTIMER_H_
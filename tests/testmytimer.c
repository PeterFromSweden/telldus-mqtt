#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "mytimer.h"
#include "mythread.h"
#include "asrt.h"
#include "log.h"

static void myTimerCallback(MyTimer* myTimer);
static bool cbRun = false;

int main(void)
{
  MyTimer* myTimer = MyTimer_Create(&myTimerCallback);
  int res = 0;

  MyTimer_Start(myTimer, 1000);

  MyThread_Sleep(500);
  if( cbRun == true ) res |= 1; // fail

  MyThread_Sleep(1000);
  if( cbRun == false ) res |= 1; // fail

  //---
  cbRun = false;
  MyTimer_Start(myTimer, 10000);
  MyThread_Sleep(2000);
  MyTimer_Start(myTimer, 1000);
  MyThread_Sleep(500);
  if( cbRun == true ) res |= 1; // fail

  MyThread_Sleep(1000);
  if( cbRun == false ) res |= 1; // fail

  MyTimer_Destroy(myTimer);

  return res;
}

static void myTimerCallback(MyTimer* myTimer)
{
  cbRun = true;
}

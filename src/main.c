#include <stdio.h>
#include <string.h>
#include <time.h>
#include <mosquitto.h>
#include <mqtt_protocol.h>
#include "criticalsection.h"
#include "mythread.h"
#include "telldusclient.h"
#include "mqttclient.h"
#include "log.h"
#include "config.h"


static Config* config;
static TelldusClient* telldusclient;
static MqttClient* mqttclient;

static struct mosquitto* mosq;

static char serial[12];
static char firmware[12];

static int evtDevice;
static int evtDeviceChange;
static int evtRawDevice;
static int evtController;
static int evtSensor;

static bool connected = false;
static CriticalSection* criticalsectionPtr;

int main(int argc, char *argv[])
{
  printf("main\r\n");
  Log_Init(TM_LOG_CONSOLE, "telldus-mqtt", TM_LOG_DEBUG);
  //Log_Init(TM_LOG_SYSLOG, "telldus-mqtt", TM_LOG_DEBUG);

  config = Config_GetInstance();
  Config_Load(config, "telldus-core-mqtt.json");

  telldusclient = TelldusClient_GetInstance();

  while(1)
  {
    if( !TelldusClient_IsConnected(telldusclient) )
    {
      TelldusClient_Connect(telldusclient);
    }
    else
    {
      mqttclient = MqttClient_GetInstance();
      if( !MqttClient_IsConnected(mqttclient) )
      {
        MqttClient_Connect(mqttclient);
      }
    }
    
    MyThread_Sleep(1000);
  }

  return 0;

  /* Run the network loop in a background thread, this call returns quickly. */
  //rc = mosquitto_loop_forever(mosq, -1, 1);
  CriticalSection_Enter(criticalsectionPtr);
  int rc;
  rc = mosquitto_loop_start(mosq);
  if ( rc != MOSQ_ERR_SUCCESS ) {
    mosquitto_destroy(mosq);
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
    CriticalSection_Leave(criticalsectionPtr);
    return 1;
  }
  CriticalSection_Leave(criticalsectionPtr);


  /* At this point the client is connected to the network socket, but may not
 * have completed CONNECT/CONNACK.
 * It is fairly safe to start queuing messages at this point, but if you
 * want to be really sure you should wait until after a successful call to
 * the connect callback.
 * In this case we know it is 1 second before we start publishing.
 */
  while ( !connected )
  {
    MyThread_Sleep(100);
  }
  CriticalSection_Enter(criticalsectionPtr);
  mosquitto_subscribe(mosq, NULL, "telldus/device/#", 0); // Always listen for unconfigured telldus topics
  mosquitto_subscribe(mosq, NULL, "house/christmastree", 0);
  /*
  for ( int i = 1; i < 10; i++ )
  {
    char telldusTopic[70];
    char mqttTopic[70];
    snprintf(telldusTopic, sizeof(telldusTopic), "telldus/device/%i", i);
    if ( !Config_GetTopicTranslation(&config, "telldus", telldusTopic, "mqtt", mqttTopic, sizeof(mqttTopic) - 1) )
    {
      // strcat(mqttTopic, "/#");
      mosquitto_subscribe(mosq, NULL, mqttTopic, 0);
      printf("Subscribing to %s\r\n", mqttTopic);
    }
  }
  */
  CriticalSection_Leave(criticalsectionPtr);

  while ( 1 )
  {
    // Events!
    //publish_sensor_data(mosq);
    CriticalSection_Enter(criticalsectionPtr);
    //mosquitto_loop(mosq, -1, 1);
    CriticalSection_Leave(criticalsectionPtr);
    
    MyThread_Sleep(100);
  }

  CriticalSection_Enter(criticalsectionPtr);
  mosquitto_lib_cleanup();
  CriticalSection_Leave(criticalsectionPtr);
  CriticalSection_Destroy(criticalsectionPtr);

  return 0;
}
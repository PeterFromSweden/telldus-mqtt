#include <stdio.h>
#include <string.h>
#include <time.h>
#include <mosquitto.h>
#include <mqtt_protocol.h>
#include <telldus-core.h>
#include "criticalsection.h"
#include "mythread.h"
#include "config.h"

static void telldusDeviceEvent(int deviceId, int method, const char* data, int callbackId, void* context);
static void telldusDeviceChangeEvent(int deviceId, int changeEvent, int changeType, int callbackId, void* context);
static void telldusRawDeviceEvent(const char* data, int controllerId, int callbackId, void* context);
static void telldusSensorEvent(const char* protocol, const char* model, int id, int dataType, const char* value, int timestamp, int callbackId, void* context);
static void telldusControllerEvent(int controllerId, int changeEvent, int changeType, const char* newValue, int callbackId, void* context);

static Config config;
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

/* Callback called when the client receives a CONNACK message from the broker. */
static void on_connect(struct mosquitto* mosq, void* obj, int reason_code)
{
  /* Print out the connection result. mosquitto_connack_string() produces an
   * appropriate string for MQTT v3.x clients, the equivalent for MQTT v5.0
   * clients is mosquitto_reason_string().
   */
  printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
  if ( reason_code != 0 ) {
    /* If the connection fails for any reason, we don't want to keep on
     * retrying in this example, so disconnect. Without this, the client
     * will attempt to reconnect. */
    mosquitto_disconnect(mosq);
  }

  /* You may wish to set a flag here to indicate to your application that the
   * client is now connected. */
  connected = true;
}


/* Callback called when the client knows to the best of its abilities that a
 * PUBLISH has been successfully sent. For QoS 0 this means the message has
 * been completely written to the operating system. For QoS 1 this means we
 * have received a PUBACK from the broker. For QoS 2 this means we have
 * received a PUBCOMP from the broker. */
static void on_publish(struct mosquitto* mosq, void* obj, int mid)
{
  //printf("Message with mid %d has been published.\n", mid);
}

// Callback
int on_message(struct mosquitto* mosq, void* userdata, const struct mosquitto_message* msg)
{
  printf("NYI: mqtt message %s %s (%d)\n", msg->topic, (const char*)msg->payload, msg->payloadlen);

  char telldusTopic[70];
  if ( Config_GetTopicTranslation(&config, "mqtt", msg->topic, "telldus", telldusTopic, sizeof(telldusTopic)) )
  {
    // No match
    strncpy(telldusTopic, msg->topic, sizeof(telldusTopic));
  }
  
  int rc;
  if ( msg->payloadlen == 2 )
  {
    // on
    rc = tdTurnOn(1);
  }
  else
  {
    // on
    rc = tdTurnOff(1);
  }
  if ( rc != TELLSTICK_SUCCESS )
  {
    fprintf(stderr, "Turn on/off %s\r\n", tdGetErrorString(rc));
  }
  return 0;
}


static int getTelldusController(void)
{
  int res = TELLSTICK_SUCCESS;
  int id;
  int type;
  int available = 0;
  char str[32];
  int ctrlres;
  
  while ( !available && res == TELLSTICK_SUCCESS )
  {
    res = tdController(&id, &type, str, sizeof(str), &available);
    ctrlres = tdControllerValue(id, "serial", serial, sizeof(serial));
    ctrlres = tdControllerValue(id, "firmware", firmware, sizeof(firmware));
  }
  return available;
}

static void telldusDeviceEvent(int deviceId, int method, const char* data, int callbackId, void* context)
{
  printf("NYI:telldusDeviceEvent %i, %i, %s, %i\r\n", deviceId, method, data, callbackId);
}

static void telldusDeviceChangeEvent(int deviceId, int changeEvent, int changeType, int callbackId, void* context)
{
  printf("NYI:telldusDeviceChangeEvent %i, %i, %i, %i\r\n", deviceId, changeEvent, changeType, callbackId);
}

static void telldusRawDeviceEvent(const char* data, int controllerId, int callbackId, void* context)
{
  printf("NYI:telldusRawDeviceEvent %s, %i, %i\r\n", data, controllerId, callbackId);
  //telldusRawDeviceEvent class :sensor; protocol:fineoffset; id : 167; model:temperaturehumidity; humidity : 44; temp:15.8; , 2, 5
}

static void telldusSensorEvent(const char* protocol, const char* model, int id, int dataType, const char* value, int timestamp, int callbackId, void* context) 
{
  //printf("telldusSensorEvent %s, %s, %i, %i, %s, %i, %i\r\n", protocol, model, id, dataType, value, timestamp, callbackId);
  if ( !connected )
  {
    printf("telldusSensorEvent but mqtt is not connected!\r\n");
    return;
  }
  char payload[20];
  int rc;


  /* Print it to a string for easy human reading - payload format is highly
   * application dependent. */
  snprintf(payload, sizeof(payload), "%s", value);

  
  char dataDypeStr[20] = "NYI";
  switch ( dataType )
  {
  case TELLSTICK_TEMPERATURE:
    sprintf(dataDypeStr, "temp");
    break;
  case TELLSTICK_HUMIDITY:
    sprintf(dataDypeStr, "hum");
    break;
  }

  /* Publish the message
   * mosq - our client instance
   * *mid = NULL - we don't want to know what the message id for this message is
   * topic = "example/temperature" - the topic on which this message will be published
   * payloadlen = strlen(payload) - the length of our payload in bytes
   * payload - the actual payload
   * qos = 2 - publish with QoS 2 for this example
   * retain = false - do not use the retained message feature for this message
   */
  char telldusTopic[70];
  snprintf(telldusTopic, sizeof(telldusTopic), "telldus/%s/%s/%i/%s", protocol, model, id, dataDypeStr);
  char topic[70];
  if ( Config_GetTopicTranslation(&config, "telldus", telldusTopic, "mqtt", topic, sizeof(topic)) )
  {
    // No match
    strncpy(topic, telldusTopic, sizeof(topic));
  }
  CriticalSection_Enter(criticalsectionPtr);
  rc = mosquitto_publish(mosq, NULL, topic, (int) strlen(payload), payload, 0, false);
  if ( rc != MOSQ_ERR_SUCCESS ) {

    fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
  }
  else
  {
    //printf("Published %s = '%s'\r\n", topic, payload);
  }
  CriticalSection_Leave(criticalsectionPtr);
}

static void telldusControllerEvent(int controllerId, int changeEvent, int changeType, const char* newValue, int callbackId, void* context)
{
  printf("telldusControllerEvent %i, %i, %i, %s, %i\r\n", controllerId, changeEvent, changeType, newValue, callbackId);
}


int main(int argc, char *argv[])
{
  printf("main\r\n");

  criticalsectionPtr = CriticalSection_Create();
  
  Config_Init(&config);
  Config_Load(&config, "telldus-core-mqtt.json");
  
  // Telldus-core lib
  tdInit();
  if ( getTelldusController() != 1 )
  {
    fprintf(stderr, "Error: No connected telldus device?!?\r\n");
    return 1;
  }

  evtController = tdRegisterControllerEvent(&telldusControllerEvent, NULL);
  evtSensor = tdRegisterSensorEvent(&telldusSensorEvent, NULL);
  evtDevice = tdRegisterDeviceEvent(&telldusDeviceEvent, NULL);
  evtDeviceChange = tdRegisterDeviceChangeEvent(&telldusDeviceChangeEvent, NULL);
  //evtRawDevice = tdRegisterRawDeviceEvent(&telldusRawDeviceEvent, NULL);

  // Mosquitto lib
  /* Required before calling other mosquitto functions */
  CriticalSection_Enter(criticalsectionPtr);
  mosquitto_lib_init();
  int major, minor, revision;
  mosquitto_lib_version(&major, &minor, &revision);
  CriticalSection_Leave(criticalsectionPtr);
  printf("mosquitto version %d.%d.%d\r\n", major, minor, revision);

  /* Create a new client instance.
   * id = NULL -> ask the broker to generate a client id for us
   * clean session = true -> the broker should remove old sessions when we connect
   * obj = NULL -> we aren't passing any of our private data for callbacks
   */
  CriticalSection_Enter(criticalsectionPtr);
  mosq = mosquitto_new(serial, true, NULL);
  if ( mosq == NULL ) {
    fprintf(stderr, "Error: Out of memory.\n");
    return 1;
  }
  CriticalSection_Leave(criticalsectionPtr);

  /* Configure callbacks. This should be done before connecting ideally. */
  CriticalSection_Enter(criticalsectionPtr);
  mosquitto_connect_callback_set(mosq, on_connect);
  mosquitto_publish_callback_set(mosq, on_publish);
  mosquitto_message_callback_set(mosq, on_message);
  CriticalSection_Leave(criticalsectionPtr);

  /* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
   * This call makes the socket connection only, it does not complete the MQTT
   * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
   * mosquitto_loop_forever() for processing net traffic. */
  
  char username[20];
  char password[20];
  Config_GetStr(&config, "user", username, sizeof(username));
  Config_GetStr(&config, "pass", password, sizeof(password));
  CriticalSection_Enter(criticalsectionPtr);
  int rc = mosquitto_username_pw_set(mosq, username, password);
  if ( rc != MOSQ_ERR_SUCCESS ) {
    mosquitto_destroy(mosq);
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
    CriticalSection_Leave(criticalsectionPtr);
    return 1;
  }
  CriticalSection_Leave(criticalsectionPtr);

  char host[40];
  int port;
  Config_GetStr(&config, "host", host, sizeof(host));
  Config_GetInt(&config, "port", &port);
  CriticalSection_Enter(criticalsectionPtr);
  rc = mosquitto_connect(mosq, host, port, 60);
  if ( rc == MOSQ_ERR_ERRNO )
  {
    printf("Is %s:%i a MQTT broker?\r\n", host, port);
  }
  if ( rc != MOSQ_ERR_SUCCESS ) {
    mosquitto_destroy(mosq);
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
    CriticalSection_Leave(criticalsectionPtr);
    return 1;
  }
  CriticalSection_Leave(criticalsectionPtr);

  /* Run the network loop in a background thread, this call returns quickly. */
  //rc = mosquitto_loop_forever(mosq, -1, 1);
  CriticalSection_Enter(criticalsectionPtr);
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
  mosquitto_subscribe(mosq, NULL, "house/cristmastree", 0);
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
    
    Sleep(100);
  }

  CriticalSection_Enter(criticalsectionPtr);
  mosquitto_lib_cleanup();
  CriticalSection_Leave(criticalsectionPtr);
  CriticalSection_Destroy(criticalsectionPtr);

  return 0;
}
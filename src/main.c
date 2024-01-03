#include <stdio.h>
#include <string.h>
#include <mosquitto.h>
#include <mqtt_protocol.h>
#include <telldus-core.h>
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
  char payload[20];
  int temp;
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
  if ( Config_GetTopicTranslation(&config, telldusTopic, topic, sizeof(topic)) )
  {
    // No match
    strncpy(topic, telldusTopic, sizeof(topic));
  }
  rc = mosquitto_publish(mosq, NULL, topic, strlen(payload), payload, 0, false);
  if ( rc != MOSQ_ERR_SUCCESS ) {

    fprintf(stderr, "Error publishing: %s\n", mosquitto_strerror(rc));
  }
  else
  {
    //printf("Published %s = '%s'\r\n", topic, payload);
  }
}

static void telldusControllerEvent(int controllerId, int changeEvent, int changeType, const char* newValue, int callbackId, void* context)
{
  printf("telldusControllerEvent %i, %i, %i, %s, %i\r\n", controllerId, changeEvent, changeType, newValue, callbackId);
}


int main(int argc, char *argv[])
{
  printf("main\r\n");
  
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
  mosquitto_lib_init();
  int major, minor, revision;
  mosquitto_lib_version(&major, &minor, &revision);
  printf("mosquitto version %d.%d.%d\r\n", major, minor, revision);

  /* Create a new client instance.
   * id = NULL -> ask the broker to generate a client id for us
   * clean session = true -> the broker should remove old sessions when we connect
   * obj = NULL -> we aren't passing any of our private data for callbacks
   */
  mosq = mosquitto_new(serial, true, NULL);
  if ( mosq == NULL ) {
    fprintf(stderr, "Error: Out of memory.\n");
    return 1;
  }

  /* Configure callbacks. This should be done before connecting ideally. */
  mosquitto_connect_callback_set(mosq, on_connect);
  mosquitto_publish_callback_set(mosq, on_publish);

  /* Connect to test.mosquitto.org on port 1883, with a keepalive of 60 seconds.
   * This call makes the socket connection only, it does not complete the MQTT
   * CONNECT/CONNACK flow, you should use mosquitto_loop_start() or
   * mosquitto_loop_forever() for processing net traffic. */
  
  int rc;
  char username[20];
  char password[20];
  Config_GetStr(&config, "user", username, sizeof(username));
  Config_GetStr(&config, "pass", password, sizeof(password));
  rc = mosquitto_username_pw_set(mosq, username, password);
  if ( rc != MOSQ_ERR_SUCCESS ) {
    mosquitto_destroy(mosq);
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
    return 1;
  }

  char host[40];
  int port;
  Config_GetStr(&config, "host", &host, sizeof(host));
  Config_GetInt(&config, "port", &port);
  rc = mosquitto_connect(mosq, host, port, 60);
  if ( rc != MOSQ_ERR_SUCCESS ) {
    mosquitto_destroy(mosq);
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
    return 1;
  }

  /* Run the network loop in a background thread, this call returns quickly. */
  rc = mosquitto_loop_start(mosq);
  if ( 0 && rc != MOSQ_ERR_SUCCESS ) {
    mosquitto_destroy(mosq);
    fprintf(stderr, "Error: %s\n", mosquitto_strerror(rc));
    return 1;
  }


  /* At this point the client is connected to the network socket, but may not
 * have completed CONNECT/CONNACK.
 * It is fairly safe to start queuing messages at this point, but if you
 * want to be really sure you should wait until after a successful call to
 * the connect callback.
 * In this case we know it is 1 second before we start publishing.
 */
  while ( 1 )
  {
    // Events!
    //publish_sensor_data(mosq);

  }

  mosquitto_lib_cleanup();

  return 0;
}
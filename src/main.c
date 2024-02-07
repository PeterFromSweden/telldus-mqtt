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
#include "asrt.h"
#include "version.h"


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

static int log_dest = TM_LOG_SYSLOG;
static int log_level = TM_LOG_INFO;
static bool log_time = false;
static bool log_raw = false;
static bool sig_exit = false;

static void atexitFunction(void);
static void signalHandler(int sig);

int main(int argc, char *argv[])
{
  int arg = 1;
  while( arg < argc )
  {
    if( strcmp(argv[arg], "--nodaemon") == 0 )
    {
      log_dest = TM_LOG_CONSOLE;
    }
    else if( strcmp(argv[arg], "--debug") == 0 )
    {
      log_level = TM_LOG_DEBUG;
    }
    else if( strcmp(argv[arg], "--logtime") == 0 )
    {
      log_time = true;
    }
    else if( strcmp(argv[arg], "--raw") == 0 )
    {
      log_raw = true;
    }
    else
    {
      printf("telldus-mqtt [--nodaemon] [--debug] [--logtime] [--raw]\n");
      exit(1);
    }
    arg++;
  }
  Log_Init(log_dest, "telldus-mqtt", log_level, log_time);
  Log(TM_LOG_INFO, "telldus-mqtt %s", TELLDUS_MQTT_VERSION);

  config = Config_GetInstance();
  ASRT( !Config_Load(config, "telldus-mqtt.json") );

  telldusclient = TelldusClient_GetInstance();
  TelldusClient_SetLogRaw( telldusclient, log_raw );
  TelldusClient_Connect(telldusclient);
  
  mqttclient = MqttClient_GetInstance();
  MqttClient_Connect(mqttclient);

  atexit(atexitFunction);
  signal(SIGTERM, signalHandler);
	signal(SIGINT,  signalHandler);
	signal(SIGPIPE, signalHandler);
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = SA_NOCLDWAIT;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGCHLD, &sa, NULL) == -1) 
  {
		Log(TM_LOG_ERROR, "Could not set the SA_NOCLDWAIT flag. We will be creating zombie processes!");
    exit(1);
	}
  
  while( !sig_exit )
  {
    MyThread_Sleep(1000);
  }

  return 0;
}

static void signalHandler(int sig)
{
  switch(sig)
  {
    case SIGHUP:
      Log(TM_LOG_WARNING, "Received SIGHUP signal.");
      break;
    case SIGTERM:
      Log(TM_LOG_WARNING, "Received SIGTERM signal.");
      break;
    case SIGINT:
      Log(TM_LOG_WARNING, "Received SIGINT signal.");
      break;
  }
  sig_exit = true;
}

static void atexitFunction(void)
{
  Log(TM_LOG_DEBUG, "Atexit - Exiting");
  if( mqttclient )
  {
    MqttClient_Destroy( mqttclient );
    mqttclient = NULL;
  }

  if( telldusclient )
  {
    TelldusClient_Destroy( telldusclient );
    telldusclient = NULL;
  }
  Log(TM_LOG_DEBUG, "Atexit - Bye");
}
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
#include "status.h"
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
static const char* config_path = NULL;
static const char* status_path = NULL;

static void atexitFunction(void);
static void signalHandler(int sig);
static bool windowsSignalHandler(int sig);

int main(int argc, char *argv[])
{
  // No buffer: the log is visible at once, also in a pipe or a file
  setvbuf(stdout, NULL, _IONBF, 0);

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
    else if( strcmp(argv[arg], "--config") == 0 && arg + 1 < argc )
    {
      arg++;
      config_path = argv[arg];
    }
    else if( strcmp(argv[arg], "--status") == 0 && arg + 1 < argc )
    {
      arg++;
      status_path = argv[arg];
    }
    else if( strcmp(argv[arg], "--version") == 0 )
    {
      printf("telldus-mqtt %s\n", TELLDUS_MQTT_VERSION);
      exit(0);
    }
    else
    {
      printf("telldus-mqtt %s\n", TELLDUS_MQTT_VERSION);
      printf("telldus-mqtt [--nodaemon] [--debug] [--logtime] [--raw] [--config <path>] [--status <path>] [--version]\n");
      exit(1);
    }
    arg++;
  }
  Log_Init(log_dest, "telldus-mqtt", log_level, log_time);
  Log(TM_LOG_INFO, "telldus-mqtt %s", TELLDUS_MQTT_VERSION);

  config = Config_GetInstance();
  int configError = config_path != NULL ?
    Config_LoadFile(config, config_path) :
    Config_Load(config, "telldus-mqtt.json");
  if( configError )
  {
    Log(TM_LOG_ERROR, "Cannot load config");
    exit(1);
  }

  if( status_path != NULL )
  {
    if( Status_Init(status_path) )
    {
      exit(1);
    }
    Status_Write(); // The file exists also if telldusd is not reachable
  }
  // Early, to write the status file also on exit(1) when connecting
  atexit(atexitFunction);

  telldusclient = TelldusClient_GetInstance();
  TelldusClient_SetLogRaw( telldusclient, log_raw );
  ASRT( TelldusClient_Connect(telldusclient) == 0 );
  
  mqttclient = MqttClient_GetInstance();
  MqttClient_Connect(mqttclient);

# ifndef WIN32
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
# else
  SetConsoleCtrlHandler( (PHANDLER_ROUTINE) windowsSignalHandler, TRUE );
# endif
  
  while( !sig_exit )
  {
    if( !MqttClient_IsConnected( mqttclient ) )
    {
      Log(TM_LOG_ERROR, "MQTT is not connected");
      sig_exit = true;
    }

    if( !TelldusClient_IsConnected( telldusclient ) )
    {
      Log(TM_LOG_ERROR, "Telldus is not connected");
      sig_exit = true;
    }

    Status_Poll();
    MyThread_Sleep(1000);
  }

  return 0;
  // atexitFunction follows
}

#ifndef WIN32
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
    default:
      Log(TM_LOG_ERROR, "Uknown signal received.");
      break;
  }
  sig_exit = true;
}
#else
static bool windowsSignalHandler(int sig)
{
  if( sig == CTRL_C_EVENT )
  {
    Log(TM_LOG_WARNING, "Received CTRL-C event.");
  }
  else
  {
    Log(TM_LOG_WARNING, "Received %i event.", sig);
  }
  sig_exit = true;
  return true;
}

#endif

static void atexitFunction(void)
{
  // Last status, with the connection state at exit
  Status_Stop();

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
}
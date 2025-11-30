# Telldus-MQTT Architecture Review & Improvement Plan

## Executive Summary
This document analyzes the telldus-mqtt architecture, identifies current issues, and proposes improvements for enhanced reliability, maintainability, and functionality.

**Document Version:** 1.0  
**Date:** November 30, 2025  
**Status:** Draft for Review

---

## Current Architecture Overview

### System Components

```
┌─────────────────┐       ┌──────────────────┐       ┌─────────────────┐
│  Telldus Core   │◄──────┤  telldus-mqtt    │──────►│  MQTT Broker    │
│  (telldusd)     │       │   (Bridge)       │       │  (mosquitto)    │
└─────────────────┘       └──────────────────┘       └─────────────────┘
        │                          │                          │
        │                          │                          │
        ▼                          ▼                          ▼
  RF 433MHz               JSON Config                  Home Assistant
  Devices/Sensors         Mappings                     MQTT Discovery

```

### Core Modules

1. **main.c** - Entry point, signal handling, initialization orchestration
2. **telldusclient.c** - Interface to telldus-core library (tdInit, event registration)
3. **telldusdevice.c** - Device state management (switches, dimmers)
4. **telldussensor.c** - Sensor data management (temperature, humidity, etc.)
5. **mqttclient.c** - MQTT publishing/subscribing, Home Assistant discovery
6. **config.c / configjson.c** - Configuration file parsing and topic translation
7. **stringutils.c** - String manipulation utilities (word replacement)
8. **mytimer_*.c** - Platform-specific timer implementation for retries/offline detection

### Data Flow

**Sensor Events:**
```
RF Sensor → telldusd → telldus-core API → telldusclient callback →
  TelldusSensor_Create() → MqttClient_AddSensor() →
    Generate HA discovery topic → Apply JSON mapping (if exists) →
      Publish retained config → Publish sensor value
```

**Device Commands:**
```
MQTT command topic ← mosquitto ← Home Assistant
  ↓
on_message() → TelldusDevice_GetTopic() → TelldusDevice_Action() →
  tdDim/tdTurnOn/tdTurnOff → RF transmission
  ↓
MyTimer schedules retry (1 second later)
```

### Configuration Files

- `/etc/telldus-mqtt/telldus-mqtt.json` - MQTT broker credentials + topic-translation mappings (telldus→mqtt)
- `/etc/telldus-mqtt/telldus-mqtt-homeassistant.json` - Home Assistant MQTT discovery templates (applied to default or user-defined topics)

### Key Design Patterns

- **Singleton Pattern:** All major components (Config, TelldusClient, MqttClient) use GetInstance()
- **Linked List:** Devices and sensors stored in manually-managed linked lists
- **Event-Driven:** Callbacks from telldus-core trigger MQTT publications
- **Retained Messages:** Home Assistant discovery configs published with retain=true
- **Word Boundary Replacement:** Topic mapping uses delimiter-aware string replacement

---

## Current Issues Analysis

### Issue #6: Retained MQTT Topics Not Deleted When Remapped

**Severity:** Medium  
**Impact:** MQTT broker accumulates stale retained topics, causing confusion and storage bloat

#### Problem Description
When a sensor first publishes data without a JSON mapping, telldus-mqtt generates a default topic like:
```
homeassistant/sensor/1234567/fineoffset-temperaturehumidity-231-Temperature/config
```

This is published with `retain=true`. Later, when a mapping is added:
```json
{
  "fineoffset-temperaturehumidity-231-Temperature": "living_room_temp"
}
```

The new topic is published:
```
homeassistant/sensor/1234567/living_room_temp/config
```

But the old default topic **remains retained forever** on the MQTT broker.

#### Root Cause
`MqttClient_AddSensor()` in `mqttclient.c` (lines 128-198):
- Generates default topic from sensor properties
- Applies translation if mapping exists
- Publishes new topic with retain=true
- **Never publishes empty payload to old topic**

#### Current Code Flow
```c
void MqttClient_AddSensor(MqttClient* self, TelldusSensor* sensor) {
  // Generate default topic structure
  ConfigJson_LoadTemplate(&cj, "telldus-mqtt-homeassistant.json");
  ConfigJson_ReplaceWordList(&cj, ...); // Creates default topic
  
  // Apply user translation
  char* name = Config_GetTopicTranslation(self->config, ConfigJson_GetContent(&cj));
  if (name != NULL) {
    ConfigJson_SetStringFromPropList(&cj, ..., "name", name);
    // Problem: Old topic is never deleted!
  }
  
  // Publish new config
  mosquitto_publish(self->mosq, NULL, topic, strlen(payload), payload, 0, true);
}
```

#### Proposed Solution

**Strategy: Always Publish Empty Payload to Default Topic When Mapped (Recommended)**

Since the design doesn't track state changes, we simplify by **always** publishing an empty retained message to the default-generated topic whenever a user-defined mapping exists. This ensures cleanup without needing change detection.

**Implementation:**
```c
void MqttClient_AddSensor(MqttClient* self, TelldusSensor* sensor) {
  // Generate default topic structure
  ConfigJson_LoadTemplate(&cj, "telldus-mqtt-homeassistant.json");
  ConfigJson_ReplaceWordList(&cj, ...); // Creates default topic
  
  // Extract default config topic BEFORE applying translation
  char default_topic[TM_TOPIC_SIZE];
  strcpy(default_topic, ConfigJson_GetStringFromPropList(&cj, 
    (const char * const []) {"sensor-config", "topic", ""}));
  
  // Apply user translation
  char* name = Config_GetTopicTranslation(self->config, ConfigJson_GetContent(&cj));
  
  if (name != NULL) {
    // User-defined mapping exists
    ConfigJson_ParseContent(&cj);
    ConfigJson_SetStringFromPropList(&cj, ..., "name", name);
    
    // Publish empty payload to default topic to clear it
    mosquitto_publish(self->mosq, NULL, default_topic, 0, NULL, 0, true);
    Log(TM_LOG_DEBUG, "Cleared default retained topic: %s", default_topic);
  } else {
    // No mapping - use default topic
    ConfigJson_ParseContent(&cj);
  }
  
  // Publish new/current config
  char* final_topic = ConfigJson_GetStringFromPropList(&cj, 
    (const char * const []) {"sensor-config", "topic", ""});
  mosquitto_publish(self->mosq, NULL, final_topic, strlen(payload), 
                    payload, 0, true);
}
```

**Advantages:**
- No state tracking required (aligns with stateless design)
- No change detection logic needed
- Works correctly even if mapping added after initial discovery
- Idempotent - safe to run on every restart

**Alternative: External Cleanup Script (Optional Supplement)**

The existing `/home/vagrant/telldus-mqtt/src/cleanup_telldus_retained.sh` could be enhanced to run as a one-time migration tool for existing deployments with accumulated retained topics.

**Effort:** Small (1 day for primary solution)  
**Testing:** Requires MQTT broker monitoring and Home Assistant validation

---

### Issue #5: Robust RF Transmission Set Commands

**Severity:** Medium  
**Impact:** Unreliable device control due to RF interference/collisions

#### Problem Description
RF 433MHz has no collision detection or acknowledgment. Multiple simultaneous transmissions or noise can cause commands to fail silently. Currently:
- Single transmission attempt via `tdTurnOn()`, `tdTurnOff()`, `tdDim()`
- MyTimer schedules **one retry** after 1 second
- No randomization to avoid collision with other devices

#### Current Code
```c
void TelldusDevice_Action(TelldusDevice* self, const char* action) {
  if (compareStringsIgnoreCase(action, self->lastAction)) {
    Log(TM_LOG_DEBUG, "Action %s already sent => resending", action);
  } else {
    strcpy(self->lastAction, action);
    MyTimer_Start(self->myTimer, 1000); // Fixed 1s retry
  }
  
  if (strcmp(action, "ON") == 0) {
    tdTurnOn(self->device_number);
  } else if (strcmp(action, "OFF") == 0) {
    tdTurnOff(self->device_number);
  } else {
    tdDim(self->device_number, atoi(action));
  }
}
```

#### Investigation Required
**Question:** Does telldus-core (`tdTurnOn`, etc.) already implement retry logic?

**Test Plan:**
1. Review telldus-core source: `telldus-core/client/telldus-core.cpp`
2. Check for `TELLSTICK_ERROR_*` return codes
3. Monitor RF transmissions with SDR receiver
4. Test deliberate interference scenarios

#### Proposed Solution (If telldus-core lacks retries)

**Multi-Retry with Exponential Backoff + Jitter:**
```c
#define MAX_RETRIES 3

typedef struct {
  char lastAction[20];
  int retryCount;
  int retryDelay; // milliseconds
  MyTimer* myTimer;
  // ... existing fields
} TelldusDevice;

void TelldusDevice_Action(TelldusDevice* self, const char* action) {
  if (strcmp(action, self->lastAction) != 0) {
    // New action - reset retry counter
    self->retryCount = 0;
    self->retryDelay = 500; // Start with 500ms
    strcpy(self->lastAction, action);
  }
  
  // Execute RF command
  int result = TELLSTICK_SUCCESS;
  if (strcmp(action, "ON") == 0) {
    result = tdTurnOn(self->device_number);
  } else if (strcmp(action, "OFF") == 0) {
    result = tdTurnOff(self->device_number);
  } else {
    result = tdDim(self->device_number, atoi(action));
  }
  
  // Schedule retry if more attempts available
  if (self->retryCount < MAX_RETRIES) {
    // Add random jitter (0-200ms) to avoid collision
    int jitter = rand() % 200;
    int delay = self->retryDelay + jitter;
    MyTimer_Start(self->myTimer, delay);
    
    self->retryCount++;
    self->retryDelay *= 2; // Exponential backoff
    
    Log(TM_LOG_DEBUG, "Scheduled retry %d/%d after %dms", 
        self->retryCount, MAX_RETRIES, delay);
  }
}
```

**Configuration Option:**
Add to `telldus-mqtt.json`:
```json
{
  "rf_retry_count": 3,
  "rf_retry_base_delay_ms": 500
}
```

**Effort:** Small-Medium (1-2 days)  
**Testing:** Requires physical RF device testing

---

### Issue #4: DeviceId >= 10 Fails on String Replace

**Severity:** High  
**Impact:** Incorrect topic translations for multi-digit device IDs

#### Problem Description
When mapping device IDs in JSON config:
```json
{
  "Device-1": "lamp_bedroom",
  "Device-10": "lamp_living_room"
}
```

The `ReplaceWords()` function in `stringutils.c` uses word-boundary detection. However, "Device-1" is a **substring** of "Device-10", causing:
1. First pass replaces "Device-1" → "lamp_bedroom"
   - Input: `homeassistant/light/Device-10/config`
   - Output: `homeassistant/light/lamp_bedroom0/config` ❌

#### Root Cause Analysis

**stringutils.c:ReplaceWords()** (lines 23-48):
```c
bool ReplaceWords(char *buffer, const char *find, const char *replace) {
  while ((pos = strstr(pos, find)) != NULL) {
    // Check for word boundaries
    if ((pos == buffer || isDelimiter(*(pos - 1))) && 
        (pos[findLen] == '\0' || isDelimiter(pos[findLen]))) {
      // Replace
    }
  }
}
```

Problem: Hyphen `-` **is defined as a delimiter** in `isDelimiter()`:
```c
static bool isDelimiter(char c) {
  return isspace(c) || strchr(",.;:?!()\"'-_+=/\\*%&$€£¥@#~^`|<>=", c) != NULL;
                                       ↑ Hyphen here
}
```

This means:
- "Device-1" matches at boundary because `-` is a delimiter
- `ReplaceWords()` correctly replaces "Device-1" in "Device-10"
- But this is **semantically wrong** for identifiers

#### Proposed Solution

**Strategy 1: Sort Mappings by Length (Descending) - Recommended**

Process longer strings first to avoid substring conflicts:
```c
void ConfigJson_ApplyTranslations(ConfigJson* self, Config* config) {
  // Get all mapping pairs
  cJSON* mappings = cJSON_GetObjectItem(self->json, "topic_mappings");
  
  // Sort by key length (descending)
  typedef struct { char* key; char* value; int keylen; } Mapping;
  Mapping sorted[MAX_MAPPINGS];
  int count = 0;
  
  cJSON* item = mappings->child;
  while (item) {
    sorted[count].key = item->string;
    sorted[count].value = item->valuestring;
    sorted[count].keylen = strlen(item->string);
    count++;
    item = item->next;
  }
  
  // Sort by keylen descending
  qsort(sorted, count, sizeof(Mapping), compare_keylen_desc);
  
  // Apply replacements in order
  for (int i = 0; i < count; i++) {
    ReplaceWords(buffer, sorted[i].key, sorted[i].value);
  }
}
```

**Strategy 2: Exact Match Context (Alternative)**

Modify `isDelimiter()` to exclude hyphens when part of identifier:
```c
bool ReplaceWordsStrict(char *buffer, const char *find, const char *replace) {
  // Require stricter boundaries: whitespace or line boundaries only
  while ((pos = strstr(pos, find)) != NULL) {
    if ((pos == buffer || isspace(*(pos - 1)) || *(pos - 1) == '/') && 
        (pos[findLen] == '\0' || isspace(pos[findLen]) || pos[findLen] == '/')) {
      // Replace
    }
  }
}
```

**Strategy 3: Use JSON Path Replacement**

Instead of string replacement in serialized JSON, manipulate cJSON objects directly:
```c
void ConfigJson_ApplyDeviceMapping(ConfigJson* self, const char* deviceId, 
                                   const char* newName) {
  cJSON* device = cJSON_GetObjectItem(self->json, "device");
  cJSON* uniqueId = cJSON_GetObjectItem(device, "unique_id");
  
  if (strstr(uniqueId->valuestring, deviceId)) {
    // Replace in structured data, not string buffer
    cJSON_SetValuestring(uniqueId, newName);
  }
}
```

**Effort:** Small (1 day)  
**Testing:** Create test cases for Device-1 through Device-20

---

## Additional Observations

### Architectural Strengths
1. **Clean separation of concerns** - telldus/mqtt/config layers well-defined
2. **Home Assistant integration** - MQTT discovery protocol correctly implemented
3. **Cross-platform** - Windows/Linux abstractions for threads, timers, logging
4. **Configurable mapping** - Flexible JSON-based topic translation

### Architectural Weaknesses

#### 1. Manual Memory Management Fragility
- Linked lists (`DeviceNode`, `SensorNode`) use `calloc()` but no cleanup on exit
- No bounds checking on string copies (e.g., `strcpy(self->protocol, protocol)`)
- Risk of memory leaks on reconnection cycles

**Recommendation:** Add device/sensor cleanup in `TelldusClient_Destroy()`:
```c
void TelldusClient_Destroy(TelldusClient *self) {
  // Free all devices
  DeviceNode* node = devices;
  while (node) {
    DeviceNode* next = node->next;
    MyTimer_Destroy(node->device.myTimer);
    free(node);
    node = next;
  }
  devices = NULL;
  
  // Free all sensors (similar)
  tdClose();
}
```

#### 2. No State Persistence
- On restart, all device/sensor state is lost
- Sensors re-publish discovery configs on every start
- No way to detect if device state changed during downtime

**Recommendation:** Optional state file `/var/lib/telldus-mqtt/state.json`:
```json
{
  "devices": {
    "1": {"last_action": "ON", "timestamp": 1701234567}
  },
  "sensors": {
    "fineoffset-temperaturehumidity-231-Temperature": {
      "last_value": "21.5",
      "last_seen": 1701234890
    }
  }
}
```

#### 3. Synchronous MQTT Operations
- All `mosquitto_publish()` calls are blocking
- No queue for offline buffering
- Telldus events during MQTT disconnect are dropped

**Recommendation:** Implement message queue:
```c
typedef struct {
  char topic[TM_TOPIC_SIZE];
  char payload[TM_PAYLOAD_SIZE];
  bool retain;
} QueuedMessage;

void MqttClient_EnqueuePublish(MqttClient* self, const char* topic, 
                                const char* payload, bool retain) {
  if (MqttClient_IsConnected(self)) {
    mosquitto_publish(self->mosq, NULL, topic, strlen(payload), 
                      payload, 0, retain);
  } else {
    // Add to queue (circular buffer)
    queue_add(&self->messageQueue, topic, payload, retain);
  }
}

void on_connect(...) {
  // Flush queued messages
  while (!queue_empty(&self->messageQueue)) {
    QueuedMessage* msg = queue_pop(&self->messageQueue);
    mosquitto_publish(self->mosq, NULL, msg->topic, ...);
  }
}
```

#### 4. Fail-Fast Design Philosophy: Crash and Let Daemon Recover

**Current Approach:** The codebase intentionally calls `exit(1)` on critical failures:
- MQTT broker connection failure (mqttclient.c:89)
- Telldus-core connection failure (telldusclient.c:38)
- Configuration file parsing errors
- Memory allocation failures

**Design Rationale:**
1. **Simplicity:** Avoids complex retry state machines and error propagation
2. **External orchestration:** Delegates recovery to systemd/OpenWrt init system
3. **Clean slate:** Each restart ensures fresh initialization without accumulated error state
4. **Separation of concerns:** Application focuses on business logic; supervisor handles availability
5. **Predictable behavior:** Easier to debug than half-working degraded states

**Evidence of Intent:**
```c
// mqttclient.c:89
if ( rc == MOSQ_ERR_ERRNO ) {
  if( !self->mutelog ) {
    Log(TM_LOG_ERROR, "Failed to connect to MQTT broker %s:%i", 
        Config_GetStrPtr(self->config, "host"), 
        Config_GetInt(self->config, "port"));
    exit(1);  // Intentional crash
    self->mutelog = true;
  }
}
```

**Advantages:**
- **Reliability:** systemd restart is battle-tested (used by thousands of services)
- **No state leaks:** Memory leaks become irrelevant (process terminates)
- **Observable:** Clear exit logs in journal vs. silent degradation
- **Resource cleanup:** OS handles all file descriptor/socket cleanup

**Trade-offs:**
- **Restart latency:** ~1-2 seconds downtime per crash (acceptable for home automation)
- **Event loss:** Telldus events during restart are dropped (RF sensors will retransmit)
- **Restart storms:** Rapid repeated failures can cause log spam (mitigated by watchdog)

**Comparison to Alternative Approach:**

| Aspect | Fail-Fast (Current) | Internal Retry |
|--------|---------------------|----------------|
| Code complexity | Low (~50 lines) | High (~300 lines) |
| State management | None needed | Complex retry state |
| Debugging | Clear exit codes | Hidden degraded states |
| Recovery guarantee | systemd handles it | Custom logic may fail |
| Restart overhead | 1-2 seconds | 0 seconds |
| Memory leaks | Irrelevant | Must be zero |

**Recommendation: Keep Fail-Fast Design**

The current approach is **architecturally sound** for this use case:
- Home automation tolerates brief restarts
- systemd provides better supervision than custom retry logic
- Diagnostic collection system (recently added) tracks restart reasons
- External watchdog (telldus-mqtt-watchdog.sh) handles hung processes

**Optional Enhancement:** Add structured exit codes for diagnostics:
```c
#define EXIT_MQTT_CONNECT_FAILED    10
#define EXIT_TELLDUS_CONNECT_FAILED 11
#define EXIT_CONFIG_PARSE_FAILED    12
#define EXIT_OUT_OF_MEMORY          13

// Then in main.c
Log(TM_LOG_ERROR, "Failed to connect to MQTT broker");
exit(EXIT_MQTT_CONNECT_FAILED);
```

This allows `analyze-logs.sh` to categorize exit reasons more precisely without changing the crash-and-recover philosophy.

**Effort:** Keep as-is (0 days), or add exit codes (0.5 days)

#### 5. Sensor Offline Detection Timing
- `mytimer_*.c` hardcoded to 5-minute offline threshold
- No configuration option per sensor type
- Battery-powered sensors may need longer timeout

**Recommendation:** Add to homeassistant JSON:
```json
{
  "sensor-config-content": {
    "availability": {
      "topic": "...",
      "offline_timeout_seconds": 300
    }
  }
}
```

---

## Diagnostic Collection System (Recently Added)

### Components
- **collect-telldus-mqtt.sh** - BusyBox-compatible collector for OpenWrt
- **setup-ssh-keys.sh** - SSH key provisioning automation
- **collect-from-devices.sh** - Multi-device orchestration
- **analyze-logs.sh** - Restart reason categorization

### Findings from Initial Collection (5 devices)
- **192.168.12.228:** 7 watchdog restarts (message timeout) → Timeout increased to 900s
- **10.0.0.23:** 2 MQTT connection issues
- **Others:** Stable operation

### Deferred Issue: Sensor Offline Events
Multiple sensors showing frequent offline/online cycles. Possible causes:
1. RF signal quality issues (distance, interference)
2. Battery voltage drops
3. Sensor hardware malfunction
4. Incorrect offline timeout threshold

**Recommendation:** Add sensor signal quality tracking:
```c
typedef struct {
  int rssi; // If available from telldus-core
  int consecutive_failures;
  time_t last_success;
} SensorQuality;
```

---

## Prioritized Improvement Roadmap

### Phase 1: Critical Fixes (1-2 weeks)
1. **Issue #4 (DeviceId substring)** - High severity, low effort
   - Implement sorted replacement or strict matching
   - Add unit tests for Device-1 through Device-99
2. **Issue #6 (Retained cleanup)** - Medium severity, medium effort
   - Add previous_topic tracking
   - Implement deletion on mapping change
   - Test with Home Assistant

### Phase 2: Reliability Enhancements (2-3 weeks)
1. **Issue #5 (RF robustness)** - Investigate telldus-core behavior first
   - If needed, implement retry + jitter
   - Add configuration options
2. **Memory safety audit**
   - Add bounds checking with `strncpy()`
   - Memory leaks less critical due to fail-fast design, but still good practice
   - Run valgrind tests for completeness
3. **Structured exit codes**
   - Define exit code constants for different failure types
   - Update analyze-logs.sh to categorize by exit code
4. **MQTT message queue (Optional)**
   - Consider if queue is needed given fail-fast design
   - Events during restart are acceptable loss for home automation
   - If implemented, keep queue size small (10-20 messages max)

### Phase 3: Architecture Improvements (2-3 weeks)
1. **State persistence (Optional)**
   - Consider if needed given fail-fast design
   - Sensors re-announce on restart anyway
   - Could help with "last known value" display during sensor offline periods
2. **Enhanced diagnostics**
   - Add sensor signal quality tracking
   - Implement RF transmission logging
   - Create dashboard for fleet monitoring
3. **Performance optimization**
   - Profile MQTT publish performance
   - Consider batch publishing for multiple sensor updates

### Phase 4: Future Enhancements (Backlog)
1. **Sensor offline threshold configuration**
2. **MQTT TLS support**
3. **Prometheus metrics export**
4. **Unit test framework** (currently has test files but limited coverage)
5. **CI/CD pipeline** for multi-platform builds

---

## Testing Strategy

### Unit Tests (Expand existing tests/)
- `teststringutils` - Add DeviceId substring test cases
- `testconfig` - Test JSON mapping edge cases
- `testmqtt` - Mock MQTT broker for retained message testing

### Integration Tests
1. **RF Robustness Test**
   - Deploy 10 devices
   - Send commands simultaneously
   - Measure success rate
2. **Retained Cleanup Test**
   - Monitor MQTT broker with `mosquitto_sub -v '#'`
   - Add/modify mappings
   - Verify old topics deleted
3. **Reconnection Test**
   - Stop/start mosquitto during operation
   - Stop/start telldusd during operation
   - Verify graceful recovery

### Device Fleet Testing
- Use diagnostic collection system on 5 OpenWrt devices
- Monitor for regressions after each phase
- Track watchdog restart frequency

---

## Conclusion

The telldus-mqtt architecture is fundamentally sound with an intentional **fail-fast design philosophy**:

**Critical Issues:**
1. **DeviceId substring replacement bug (Issue #4)** - Causes incorrect mappings for Device-10, Device-11, etc.
2. **Retained MQTT topics accumulate (Issue #6)** - Default topics not cleaned when user mappings added
3. **RF transmission reliability (Issue #5)** - Need to investigate if telldus-core handles retries

**Architecture Strengths:**
- **Fail-fast design:** Intentional `exit(1)` on critical failures delegates recovery to systemd/init system
- **Simplicity:** No complex retry state machines or error propagation logic
- **Observability:** Clear exit codes and logs for diagnostic collection system
- **Separation of concerns:** Application handles business logic; supervisor handles availability

**Design Trade-offs Accepted:**
- 1-2 second restart latency on failure (acceptable for home automation)
- Event loss during restart (RF sensors retransmit, acceptable)
- Memory leak tolerance (process terminates anyway)

The prioritized roadmap addresses critical bugs first (Phase 1), then improves reliability (Phase 2), followed by optional enhancements (Phase 3). The recently implemented diagnostic collection system validates that the fail-fast approach works well in production.

**Estimated total effort:** 6-8 weeks for Phases 1-3 (reduced due to keeping fail-fast design)

---

## Document Metadata
- **Author:** Architecture Review (AI-assisted)
- **Review Status:** Draft
- **Next Review:** After Phase 1 completion
- **Related Files:**
  - `/home/vagrant/telldus-mqtt/src/stringutils.c`
  - `/home/vagrant/telldus-mqtt/src/mqttclient.c`
  - `/home/vagrant/telldus-mqtt/src/telldusdevice.c`
  - `/home/vagrant/telldus-mqtt/diagnostics/*.sh`

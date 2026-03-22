# croniot-iot

**A production-ready C++ framework for building connected IoT devices on ESP32.**

croniot-iot handles the hard parts of embedded IoT — network connectivity, MQTT messaging, server authentication, task scheduling, and sensor data streaming — so you can focus on your device's actual functionality.

Define your own tasks and sensors from scratch, or use ready-made templates and combine them freely. Start with a template, customize it, or build something entirely new — the framework adapts to your needs, not the other way around.

Built on ESP-IDF and FreeRTOS. Runs on ESP32-C6 (RISC-V).

---

## What it does

You define **what your device does** — or pick from existing templates and combine them. croniot-iot handles **everything else**:

- Connects to WiFi or LTE (SIM7600 modem) with automatic reconnection
- Authenticates with the croniot backend and registers device capabilities
- Publishes sensor data over MQTT in real-time
- Receives task commands from a mobile app and executes them
- Reports task progress back to the UI
- Persists credentials and scheduled tasks across reboots (LittleFS)
- Synchronizes device time with the server

Want a watering system? Combine `TaskWaterPlants` + `SensorBattery` + `SensorSolarPanel`. Want a smart lighting controller? Use `TaskLedSlider` + `TaskLedSwitch`. Need something that doesn't exist yet? Subclass `TaskBase` or `Sensor` and the framework integrates it seamlessly — same MQTT routing, same mobile app UI, same authentication flow.

```
┌─────────────────────────────────────────────────────────┐
│                    Your Device Code                     │
│  Custom tasks    Reusable templates    Custom sensors   │
├─────────────────────────────────────────────────────────┤
│                     croniot-iot                          │
│  ┌──────────┐  ┌──────────┐  ┌────────────────────┐    │
│  │   Task   │  │  Sensor  │  │   Authentication   │    │
│  │  System  │  │  System  │  │    & Setup          │    │
│  └────┬─────┘  └────┬─────┘  └────────┬───────────┘    │
│       │              │                 │                │
│  ┌────┴──────────────┴─────────────────┴───────────┐    │
│  │           Network Abstraction Layer              │    │
│  │        WiFi  ←→  HTTP / MQTT  ←→  SIM7600       │    │
│  └──────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────┤
│               ESP-IDF  /  FreeRTOS                      │
└─────────────────────────────────────────────────────────┘
```

## Key design decisions

**Dual network support.** WiFi and LTE (SIM7600) are interchangeable behind an abstract provider interface. Switch connectivity at init time — no business logic changes needed.

**Everything is a FreeRTOS task.** Sensors and tasks run in parallel threads. Communication between layers uses queues, not shared state. Thread-safe MQTT access via semaphores.

**Self-describing devices.** On first connection, the device registers its sensor types and task types with the server — including parameter metadata (ranges, types, units). The mobile app builds its UI dynamically from this metadata. Add a new sensor or task, and the app renders it automatically — no mobile code changes required.

**Mix and match.** Every task and sensor is a self-contained module. Reuse existing ones across projects, combine them in new ways, or create custom implementations — all through the same unified interface. A single framework powers devices as different as a watering system and a lighting controller.

**Zero-configuration for consumers.** A project using croniot-iot implements `TaskBase` for tasks, `Sensor` for sensors, calls `CommonSetup::setup()`, and the framework handles connection, authentication, registration, and message routing.

## Architecture

### Network layer

```
NetworkConnectionControllerBase (abstract)
├── WifiNetworkConnectionController     → esp_wifi
└── Sim7600NetworkConnectionController  → UART AT commands

HttpController (abstract)
├── WifiHttpController      → esp_http_client
└── Sim7600HttpController   → AT+HTTP commands

MqttController (abstract)
├── WifiMqttController      → esp-mqtt + queue-based worker
└── Sim7600MqttController   → AT+CMQTT commands
```

Runtime provider selection through `HttpProvider`, `MqttProvider`, and `NetworkConnectionProvider` singletons.

### Task system

Tasks are remotely-triggered actions. A mobile app sends a command via MQTT, the framework deserializes it and routes it to the correct `TaskBase` subclass.

```cpp
class TaskWaterPlants : public TaskBase {
    void executeTask(SimpleTaskData& data) override {
        int duration = std::stoi(data.parametersValues.at(1));
        openValve();
        startPump();
        // Report progress in real-time
        for (int i = 0; i < duration; i++) {
            reportProgress(i / (float)duration);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        stopPump();
        closeValve();
    }
};
```

Each task type declares its parameters (integer ranges, time pickers, switches, sliders) so the mobile app can render the appropriate UI controls automatically.

**MQTT topic routing:**
| Topic | Direction | Purpose |
|---|---|---|
| `/server/{device}/task_type/{uid}` | Server → Device | Task execution command |
| `/server/{device}/task_state_info_sync/{uid}` | Server → Device | State sync request |
| `/{device}/task_progress_update` | Device → Server | Real-time progress |

### Sensor system

Sensors are periodic data publishers. Subclass `Sensor`, implement `run()`, and call `sendSensorData()` — the framework handles MQTT serialization and delivery.

```cpp
class SensorBattery : public Sensor {
    void run() override {
        xTaskCreate(taskBattery, "Battery", 4096, this, 1, &handle);
    }
    static void taskBattery(void* param) {
        auto* self = static_cast<SensorBattery*>(param);
        while (true) {
            float voltage = readADC();
            self->sendSensorData(DEVICE_UUID, SENSOR_BATTERY, std::to_string(voltage));
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
};
```

### Parameter types

Tasks and sensors declare their parameters with type-safe metadata:

| Type | Class | UI Control | Example |
|---|---|---|---|
| Integer range | `ParameterInteger` | Number input | Duration: 1–600 seconds |
| Time | `ParameterTime` | Time picker | Schedule: HH:MM |
| Switch | `StatefulParameterSwitch` | Toggle | Pump: on/off |
| Slider | `StatefulParameterSlider` | Slider | Brightness: 0–100 |

### Authentication flow

```
Device boot
  → Connect to WiFi/LTE
  → Check stored credentials (LittleFS)
  → If new device: POST /api/register_client → receive deviceToken
  → POST /api/iot/login → authenticate
  → Register all sensor types (POST /api/register_sensor_type)
  → Register all task types (POST /api/register_task_type)
  → Sync time with server
  → Connect MQTT
  → Start sensors and tasks
```

## Project structure

```
components/croniot/src/
├── CommonSetup.h/cpp              # Boot orchestrator
├── AuthenticationController.h/cpp # Device registration & login
├── Storage.h/cpp                  # LittleFS persistence
├── CurrentDateTimeController.h/cpp
├── NetworkManager.h/cpp           # Connection state & config
│
├── network/
│   ├── connection_provider/       # WiFi & SIM7600 abstractions
│   ├── http/                      # HTTP client (WiFi + SIM7600)
│   ├── mqtt/                      # MQTT client (WiFi + SIM7600)
│   └── sim7600/                   # AT command interface
│
├── Tasks/
│   ├── TaskBase.h/cpp             # Abstract task with queue
│   ├── TaskController.h/cpp       # Task registry & MQTT routing
│   ├── TaskType.h/cpp             # Task metadata
│   ├── TaskData.h                 # Execution payload
│   └── TaskProgressUpdate.h/cpp   # Progress reporting
│
├── Sensors/
│   ├── Sensor.h/cpp               # Abstract sensor base
│   ├── SensorsController.h/cpp    # Sensor registry & lifecycle
│   └── SensorType.h/cpp           # Sensor metadata
│
├── Parameters/
│   ├── Parameter.h/cpp            # Base parameter class
│   ├── ParameterInteger.h/cpp
│   ├── ParameterTime.h/cpp
│   ├── StatefulParameterSwitch.h/cpp
│   └── StatefulParameterSlider.h/cpp
│
└── Messages/                      # JSON serialization
    ├── MessageRegisterDevice.h/cpp
    ├── MessageLoginRequest.h/cpp
    ├── MessageRegisterSensorType.h/cpp
    ├── MessageRegisterTaskType.h/cpp
    ├── MessageSensorData.h/cpp
    └── MessageState.h/cpp
```

## Usage

### 1. Add croniot-iot as a component

In your project's `CMakeLists.txt`:

```cmake
set(EXTRA_COMPONENT_DIRS
    path/to/croniot-iot/components/croniot
)
```

### 2. Define your tasks and sensors

```cpp
// Register task types with parameter metadata
auto waterDuration = ParameterInteger(1, "Duration", "Watering time", "s", 1, 600);
auto waterTime = ParameterTime(2, "Time", "Schedule");
TaskType waterTask(1, "Water plants", "Water plants for N seconds", {waterDuration, waterTime});
TaskController::instance().addTaskType(waterTask);

// Register sensor types
Parameter wifiParam(1, "WiFi signal", "number", "Signal strength", "dBm",
                    {{"minValue","-90"}, {"maxValue","-30"}});
SensorType wifiSensor("39", "WiFi signal", "WiFi signal level in dBm", wifiParam);
SensorsController::instance().addSensorType(wifiSensor);
```

### 3. Boot the framework

```cpp
extern "C" void app_main() {
    // Register your tasks and sensors
    TasksInitializer::initTasks();
    SensorsInitializer::initSensors();

    // Set network provider
    HttpProvider::set(new WifiHttpController());
    MqttProvider::set(new WifiMqttController());

    // Start
    UserCredentials creds("email", "password", "device_uuid", "Device Name", "Description");
    auto* wifi = new WifiNetworkConnectionController();
    CommonSetup::instance().setup(creds, wifi);
}
```

## Tech stack

| Component | Technology |
|---|---|
| Framework | ESP-IDF |
| Language | C++17 |
| RTOS | FreeRTOS |
| Storage | LittleFS |
| JSON | cJSON |
| MQTT | esp-mqtt |
| HTTP | esp_http_client |
| Build | PlatformIO |
| Targets | ESP32-C6 (RISC-V) |

## Real-world usage

[**croniot-watering-system**](https://github.com/user/croniot-watering-system) — An autonomous solar-powered plant watering system built on this framework. 5 tasks (watering sequences, pump control, LED control), 6 sensors (battery, solar panel, WiFi, RAM, system time), running on ESP32-C6 with I2C ADC and relay control.

[![Demo video](https://img.youtube.com/vi/sxZ00cP9hIw/maxresdefault.jpg)](https://www.youtube.com/watch?v=sxZ00cP9hIw)

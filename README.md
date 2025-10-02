# Smart IoT Home Automation and Security Suite (SIHASS)

## Abstract

This project delivers a robust and scalable solution for next-generation home automation and security, integrating real-time monitoring, environmental sensing, cloud-enabled automation, and multi-modal user access control. SIHASS combines an ESP8266 with fingerprint authentication, a relay-driven actuator platform (lighting, fan, AC), flame/gas/PIR sensors, gesture-based IR toggling, and a visually rich LCD dashboard. Local events like fire, gas leaks, theft/motion are detected instantly and trigger audible, visual, and cloud notifications through Blynk. The system is highly modular—supporting multiple sensors per type, fast admin menu controls, and cloud/mobile feedback, making it fit for both smart home enthusiasts and small facility security automation. Advanced features include auto lockout after fingerprint failures, buzzer/lcd alarm choreography, scheduling, and environmental comfort management for safer, smarter living spaces.

---

## Hardware Components

- **ESP8266 MCU**
- **Relay Module w/ PCF8574 (I2C)**
- **LCD Display (I2C)**
- **DHT Temperature/Humidity Sensor**
- **Flame and Gas Sensors**
- **PIR Motion Sensor**
- **IR Gesture Switch**
- **Buzzer**
- **LED indicators**
- **Adafruit Fingerprint Sensor (security)**
- **Push-buttons, resistors, wiring**

---

## Features

- Smartphone app (Blynk) for toggle, monitoring, flow/warnings, fingerprints
- Real-time environmental and security event alerts (Fire, Gas, Motion, etc.)
- IR gesture switch, relay-based smart lighting/fan/AC control
- Menu-driven admin functions over serial for configuration, fingerprint enroll/delete, status
- Safety interlocking and alarm with visual/audible (LCD, buzzer, backlight)
- Modular and extendable for more rooms/zones/actuators

---

## UML Class Diagram

```mermaid
classDiagram
class SIHASS_Controller {
+setup()
+loop()
+checkTimers()
+updateUptime()
+lcdonStart()
+handleMenu()
+toggleLights()
}
class FingerprintModule {
+enrollFingerprint()
+deleteFingerprint()
+getFingerprintID()
+getFingerprintEnroll()
}
class SensorSuite {
+dhtSensorChk()
+flameSensor()
+gasSensor()
+ldrSensor()
+pirCheck()
+IRSwitchFunction()
}
class BlynkApp {
+run()
+syncVirtual()
+virtualWrite()
+logEvent()
}
SIHASS_Controller --> FingerprintModule
SIHASS_Controller --> SensorSuite
SIHASS_Controller --> BlynkApp
```

## FLowchart

```mermaid
flowchart TD
    A[Startup] --> B[Init LCD WiFi Relays]
    B --> C[Main Loop]
    C --> D{Blynk Events / Serial Menu}
    D --Motion Fire Gas Detected--> E[Trigger Alarm and Notify]
    D --Finger Auth--> F[Grant or Deny Access]
    D --IR Gesture--> G[Toggle Lights]
    D --Timer Tick--> H[Sensor Read Update LCD Blynk]
    H --> C
    E --> C
    F --> C
    G --> C
```

---

## State Diagram

```mermaid
stateDiagram-v2
[*] --> Idle
Idle --> SenseMotion : PIR Active
Idle --> ReadDHT : Timer Interval
Idle --> DetectFire : Flame Sensor Trigger
Idle --> DetectGas : Gas Sensor Trigger
Idle --> AuthRequest : Fingerprint Presented
SenseMotion --> AlarmNotify : Motion
DetectFire --> AlarmNotify : Fire
DetectGas --> AlarmNotify : Gas
AlarmNotify --> Idle : Reset or Timeout
AuthRequest --> GrantAccess : Authorized
AuthRequest --> DenyAccess : Unauthorized
GrantAccess --> Idle
DenyAccess --> Idle
Idle --> IRSwitch : IR Switch Used
IRSwitch --> ToggleRelay : Lights Fans Other Toggle
ToggleRelay --> Idle
```

---

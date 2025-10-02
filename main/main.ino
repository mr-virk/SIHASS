#include <Blynk.h>
#include <Wire.h>
#include <dhtnew.h>
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <Adafruit_Fingerprint.h>
#include <PCF8574.h>

/* Fill-in information from Blynk Device Info here */
#define BLYNK_TEMPLATE_ID "id"
#define BLYNK_TEMPLATE_NAME "name"
#define BLYNK_AUTH_TOKEN "token"

/* Comment this out to disable prints and save space */
//#define BLYNK_PRINT Serial

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "wifi";
char pass[] = "pass";

// LCD object
LiquidCrystal_I2C lcd(0x3f, 16, 2);  //Address, columns, rows 16x02

// PCF8574 I2C address
PCF8574 pcf8574(0x20);

//Fingerprint
#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
SoftwareSerial mySerial(14, 12); //Rx, Tx
#else
#define mySerial Serial1
#endif

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

uint8_t id; //Unsigned Integar of 8 bit

// Set the number of allowed incorrect attempts before setting the flag
const int MAX_INCORRECT_ATTEMPTS = 3;

// Pin assignments
const int ldrledPin = D8;
DHTNEW dhtSensorPin(D7);  //Intialize DHT Sensor
const int buzzer = D3;
#define IRSensorPin D4

//Variables
int pirState = LOW;  // Stores the current state of the PIR sensor
int currentHumidity;
int currentTemperature;

//IR Switch Values
bool IRDetected = false;
bool lightsOn = false;
bool IRSwitch = false;

//Flags
bool relay1Status = false;
bool relay2Status = false;
bool relay3Status = false;
bool relay4Status = false;

bool motionDetected = false;  // Flag to indicate motion detection
bool pirButton;
bool systemLocked = false;

// Blynk virtual pin for displaying uptime
const int uptimePin = V3;

//PIR Motion Timer
unsigned long uptimeStartTime = 0;  // Records the start time of the device

//For Backgruound Tasks
unsigned long previousTime = 0;
const unsigned long interval = 10;  // millisecond

// Constants
const int adminMenuOption = 'm';
const int enrollOption = 'e';
const int deleteOption = 'd';
const int showSensorOption = 's';

// Variables for Menu Handeling
int option = 0;
unsigned long lastSerialInputTime = 0;

WidgetLCD lcd_blynk(V1);
String data = "";  // this will be used to store the value and will be converted into string as the lcd widget accept data in string format.
int clflag = 0;    // clear lcd flag.

//LCD Charaters Map
byte fire[8] = {
  B00000,
  B10000,
  B10100,
  B11101,
  B11111,
  B11111,
  B11111,
  B01110
};
byte drop[8] = {
  B00000,
  B00100,
  B01110,
  B11111,
  B11111,
  B11111,
  B01110,
  B00000
};
byte temp[8] = {
  B01110,
  B01010,
  B01010,
  B01010,
  B01010,
  B10001,
  B10001,
  B01110
};

void setup() {
  Serial.begin(115200); //Serial rate

  // Initialize LCD
  lcd.init(); //lcd start
  lcd.backlight(); //backlight on
  lcd.clear(); //lcd clear

  lcd.createChar(0, fire);
  lcd.createChar(1, drop);
  lcd.createChar(2, temp);

  lcd.setCursor(0, 0);
  lcd.print(" Connecting to ");
  lcd.setCursor(0, 1);
  lcd.print(" Wifi & Server ");

  delay(10);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass); //Blynk

  pinMode(IRSensorPin, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(ldrledPin, OUTPUT);

  pcf8574.pinMode(P0, OUTPUT);  //Relay 1
  pcf8574.pinMode(P1, OUTPUT);  //Relay 2
  pcf8574.pinMode(P2, OUTPUT);  //Relay 3
  pcf8574.pinMode(P3, OUTPUT);  //Relay 4
  pcf8574.pinMode(P4, INPUT);   //ldr
  pcf8574.pinMode(P5, INPUT);   //flame
  pcf8574.pinMode(P6, INPUT);   //gas
  pcf8574.pinMode(P7, INPUT);   //pir
  //Intialize PCF8574
  pcf8574.begin();

  uptimeStartTime = millis();  // Record the start time of the device
  lcdonStart();

  lcd_blynk.clear();
  lcd_blynk.print(0, 0, "Connecting"); //Column, Row
  lcd_blynk.print(0, 1, "to Wifi...");
  lcd_blynk.clear();
}

uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    while (!Serial.available());
    num = Serial.parseInt(); //to store value non-integer 1 2, 3, until a non-integear character is detected.
  }
  return num;
}

void loop() {
  unsigned long currentTime = millis();

  updateUptime();

  checkTimers();  // Call the global millis function

  // Call the menu function that handles user input and background tasks
  // set the data rate for the sensor serial port
  finger.begin(57600);

  handleMenu();

  getFingerprintID();

  Blynk.run();

  delay(10);
}

BLYNK_CONNECTED() {
  // Restore switch states from Blynk app on connection
  Blynk.syncVirtual(V1, V2, V3, V4, V5, V6); //Virtual Pins
}

void updateUptime() {
  unsigned long currentMillis = millis();
  unsigned long uptimeSeconds = (currentMillis - uptimeStartTime) / 1000;

  // Update the uptime on Blynk app
  Blynk.virtualWrite(uptimePin, uptimeSeconds);
}

void checkTimers() {
  unsigned long currentTime = millis();
  if (currentTime - previousTime >= interval) {
    previousTime = currentTime;

    // Code to execute every 3 second
    ldrSensor();
    IRSwitchFunction();
    dhtSensorChk();
    flameSensor();
    gasSensor();
    checkPIR();
    lcdDefault();
  }
}

void checkPIR() {
  pirState = pcf8574.digitalRead(P7);
  if (pirButton == true) {
    if (pirState == HIGH) {
      Blynk.logEvent("motion_detected", "Alert: Motion has been Detected!");
      motioDetected();
    }
  }
}

void ldrSensor() {
  int ldrValue = pcf8574.digitalRead(P4);  // read the input on analog pin

  if (ldrValue == HIGH) {
    digitalWrite(ldrledPin, HIGH);  // turn on LED
  } else {
    digitalWrite(ldrledPin, LOW);  // turn off LED
  }
}

void dhtSensorChk() {
  // READ DATA
  int chk = dhtSensorPin.read();

  String dhtErrorGen;

  if (chk != DHTLIB_WAITING_FOR_READ) {
    switch (chk) {
      case DHTLIB_OK:
        dhtErrorGen = chk;
        break;
      case DHTLIB_ERROR_CHECKSUM:
        dhtErrorGen = chk;
        break;
      case DHTLIB_ERROR_TIMEOUT_A:
        dhtErrorGen = chk;
        break;
      case DHTLIB_ERROR_TIMEOUT_B:
        dhtErrorGen = chk;
        break;
      case DHTLIB_ERROR_TIMEOUT_C:
        dhtErrorGen = chk;
        break;
      case DHTLIB_ERROR_TIMEOUT_D:
        dhtErrorGen = chk;
        break;
      case DHTLIB_ERROR_SENSOR_NOT_READY:
        dhtErrorGen = chk;
        break;
      case DHTLIB_ERROR_BIT_SHIFT:
        dhtErrorGen = chk;
        break;
      default:
        dhtErrorGen = chk;
        break;
    }

    currentHumidity = dhtSensorPin.getHumidity();
    currentTemperature = dhtSensorPin.getTemperature();

    Blynk.virtualWrite(V4, currentHumidity);
    Blynk.virtualWrite(V5, currentTemperature);
  }
}

void gasSensor() {
  int sensorValue = !pcf8574.digitalRead(P6);  //sensor has inverted logic
  if (sensorValue == HIGH) {
    Blynk.logEvent("gas_leakage", "Gas Leakage has been Detected!");
    alarmOnLcd();

    lcd_blynk.clear();
    lcd_blynk.print(0, 0, "GAS LEAKAGE!!!");
  }
}

void flameSensor() {
  int flameValue = !pcf8574.digitalRead(P5);
  //Serial.println(flameValue);
  if (flameValue == HIGH) {
    Blynk.logEvent("fire_alarm", "Fire has been Detected!");
    alarmOnLcd();

    lcd_blynk.clear();
    lcd_blynk.print(0, 0, "FLAME DETECTED!!!");
  }
}

//Relay 1 LIGHT
BLYNK_WRITE(V11) {
  if (param.asInt() == 1) {
    pcf8574.digitalWrite(P0, LOW);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("[  Switch - 1  ]");
    lcd.setCursor(0, 1);
    lcd.print("[      ON      ]");
    delay(100);
  } else {
    pcf8574.digitalWrite(P0, HIGH);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("[  Switch - 1  ]");
    lcd.setCursor(0, 1);
    lcd.print("[      OFF     ]");
    delay(100);
  }
}
//Relay 2 FAN
BLYNK_WRITE(V12) {
  if (param.asInt() == 1) {
    pcf8574.digitalWrite(P2, LOW);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("[  Switch - 2  ]");
    lcd.setCursor(0, 1);
    lcd.print("[      ON      ]");
    delay(100);
  } else {
    pcf8574.digitalWrite(P2, HIGH);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("[  Switch - 2  ]");
    lcd.setCursor(0, 1);
    lcd.print("[      OFF     ]");
    delay(100);
  }
}

//Pir Security
BLYNK_WRITE(V2) {
  if (param.asInt() == 1) {
    pirButton = true;
  } else if (param.asInt() == 0) {
    pirButton = false;
  }
}

//IR Switch
BLYNK_WRITE(V8) {
  if (param.asInt() == 1) {
    IRSwitch = true;
  } else if (param.asInt() == 0) {
    IRSwitch = false;
  }
}

void lcdonStart() {
  lcd.setCursor(0, 0);
  lcd.print("Home Automation");
  lcd.setCursor(0, 1);
  lcd.print("Security System");
  // Backlight control
  for (uint8_t i = 0; i < 3; i++) {
    // Turn backlight off
    lcd.noBacklight();
    tone(buzzer, 1000);
    delay(500);

    // Turn backlight on
    lcd.backlight();
    noTone(buzzer);
    delay(500);
  }
  lcd.clear();
}

void motioDetected() {
  lcd.setCursor(0, 0);
  lcd.write(1);
  lcd.setCursor(2, 0);
  lcd.print(" WARNING!!! ");
  lcd.setCursor(15, 0);
  lcd.write(1);
  lcd.setCursor(0, 1);
  lcd.print("Motion Detected!");

  lcd_blynk.clear();
  lcd_blynk.print(2, 0, " WARNING!!! ");
  lcd_blynk.print(0, 1, "Motion Detected!");

  // Backlight control
  for (uint8_t i = 0; i < 10; i++) {
    lcd.noBacklight();
    tone(buzzer, 1000);
    delay(300);

    lcd.backlight();
    noTone(buzzer);
    delay(300);
  }
  lcd.clear();
}

void alarmOnLcd() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(0);
  lcd.setCursor(2, 0);
  lcd.print(" WARNING!!! ");
  lcd.setCursor(15, 0);
  lcd.write(0);
  lcd.setCursor(0, 1);
  lcd.print(">Fire Detected!<");

  lcd_blynk.clear();
  lcd_blynk.print(2, 0, " WARNING!!! ");
  lcd_blynk.print(0, 1, ">Fire Detected!<");


  // Backlight control
  for (uint8_t i = 0; i < 30; i++) {
    lcd.noBacklight();
    tone(buzzer, 1000);
    delay(500);

    lcd.backlight();
    noTone(buzzer);
    delay(500);
  }
  lcd.clear();
}

void wrongAttempt() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[SYSTEM LOCKED!]");
  lcd.setCursor(0, 1);
  lcd.print("Alarm Activated!");

  lcd_blynk.clear();
  lcd_blynk.print(0, 0, "[SYSTEM LOCKED!]");
  lcd_blynk.print(0, 1, "Alarm Activated!");


  delay(1000);

  getFingerprintID();

  do {
    // Backlight control
    for (uint8_t i = 0; i < 12; i++) {
      lcd.noBacklight();
      tone(buzzer, 500);
      delay(300);

      lcd.backlight();
      noTone(buzzer);
      delay(300);
    }
  } while (systemLocked == false);
  lcd.clear();
}

void lcdDefault() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.write(1);
  lcd.setCursor(2, 0);
  lcd.print("Humi. (%):");  //0-9

  lcd.setCursor(0, 1);
  lcd.write(2);
  lcd.setCursor(2, 1);
  lcd.print("Temp. (C):");  //0-10

  lcd.setCursor(12, 0);
  lcd.print(currentHumidity);
  lcd.setCursor(12, 1);
  lcd.print(currentTemperature);
}

void adminMenu() {
  Serial.println();
  Serial.println("*******************************************");
  Serial.println("*Smart IoT Home Automation and Security Suite*");
  Serial.println("*******************************************");
  Serial.println("*                                          ");
  Serial.println("*             Menu Options:                ");
  Serial.println("*                                          ");
  Serial.println("*  1. Enroll Fingerprint - press 'e'       ");
  Serial.println("*  2. Delete Fingerprint - press 'd'       ");
  Serial.println("*  3. Sensor Details - press 's'           ");
  Serial.println();
  Serial.print("*     Humidity: ");
  Serial.print(currentHumidity);
  Serial.println("%                            ");
  Serial.print("*     Temperature: ");
  Serial.print(currentTemperature);
  Serial.println("°C                           ");
  Serial.println("                         ");
  Serial.println();
  Serial.print("*  Uptime: ");
  Serial.print(millis() / 1000);  // Uptime in seconds
  Serial.println(" seconds                ");
  Serial.println("*                                          ");
  Serial.println("*******************************************");
  Serial.println();
  delay(50);
}

//Menu
void handleMenu() {
  char inputChar;
  if (Serial.available() > 0) {
    inputChar = Serial.read();

    option = inputChar;

    lastSerialInputTime = millis();  // Update the time of last user input
  }

  // Execute the selected option
  switch (option) {
    case adminMenuOption:
      adminMenu();
      break;
    case enrollOption:
      enrollFingerprint();
      break;
    case deleteOption:
      deleteFingerprint();
      break;
    case showSensorOption:
      showSensorDetails();
      break;
    default:
      break;
  }

  option = 0;  // Reset the option after execution
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      //Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      //Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      //Serial.println("Imaging error");
      return p;
    default:
      //Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      //Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      //Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      //Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  static int incorrectAttempts = 0;

  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    lcd.setCursor(0, 0);
    lcd.print(" Access Denied! ");
    lcd.setCursor(0, 1);
    lcd.print(" > TRY AGAIN! < ");

    lcd_blynk.clear();
    lcd_blynk.print(0, 0, " Access Denied! ");
    lcd_blynk.print(0, 1, " > TRY AGAIN! < ");

    incorrectAttempts++;
    if (incorrectAttempts >= MAX_INCORRECT_ATTEMPTS) {
      wrongAttempt();
    }

    systemLocked = true;

    delay(1000);
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  lcd.setCursor(0, 0);
  lcd.print(" Access Granted ");
  lcd.setCursor(0, 1);
  lcd.print("  > WELCOME! <  ");

  lcd_blynk.clear();
  lcd_blynk.print(0, 0, " Access Granted ");
  lcd_blynk.print(0, 1, "  > WELCOME! <  ");

  pcf8574.digitalWrite(P3, LOW);  //Relay has inverted logic
  Blynk.virtualWrite(V6, HIGH);   // Set LED status in Blynk app to ON
  incorrectAttempts = 0;          //Reset the variable
  systemLocked = false;
  delay(3000);
  lcd.clear();
  pcf8574.digitalWrite(P3, HIGH);
  Blynk.virtualWrite(V6, LOW);  // Set LED status in Blynk app to ON

  return finger.fingerID;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;

  // found a match!
  // Serial.print("Found ID #");
  // Serial.print(finger.fingerID);
  // Serial.print(" with confidence of ");
  // Serial.println(finger.confidence);

  return finger.fingerID;
}

void enrollFingerprint() {
  delay(1000);  // Simulate some process
  Serial.println("Ready to enroll a fingerprint!");
  Serial.println("Please type in the ID # (from 1 to 127) you want to save this finger as...");

  lcd_blynk.clear();
  lcd_blynk.print(0, 0, "Ready to enroll a fingerprint!");

  id = readnumber();
  if (id == 0) {  // ID #0 not allowed, try again!
    return;
  }
  Serial.print("Enrolling ID #");
  Serial.println(id);

  lcd_blynk.clear();
  lcd_blynk.print(0, 0, "Enrolling ID #");
  lcd_blynk.print(0, 1, id);

  while (!getFingerprintEnroll())
    ;
}

uint8_t getFingerprintEnroll() {

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  Serial.println("Remove finger");

  lcd_blynk.clear();
  lcd_blynk.print(0, 0, "Remove finger");

  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");

  lcd_blynk.clear();
  lcd_blynk.print(0, 0, "Place same finger again");


  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  Serial.print("ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");

    lcd_blynk.clear();
    lcd_blynk.print(0, 0, "New Fingerprint Enrolled!");

  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  return true;
}

void deleteFingerprint() {
  // Implement the logic for deleting a fingerprint
  Serial.println("Deleting fingerprint...");

  lcd_blynk.clear();
  lcd_blynk.print(0, 0, "Deleting fingerprint...");

  delay(1000);  // Simulate some process
  Serial.println("Please type in the ID # (from 1 to 127) you want to delete...");

  lcd_blynk.print(0, 0, "Please type in the ID # (from 1 to 127) you want to delete...");

  uint8_t id = readnumber();
  if (id == 0) {  // ID #0 not allowed, try again!
    return;
  }

  Serial.print("Deleting ID #");
  Serial.println(id);

  lcd_blynk.clear();
  lcd_blynk.print(0, 0, "Deleting ID #");
  lcd_blynk.print(0, 0, id);

  deleteFingerprint(id);
}

uint8_t deleteFingerprint(uint8_t id) {
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK) {
    Serial.println("Deleted!");

    lcd_blynk.clear();
    lcd_blynk.print(0, 0, "Fingerprint Deleted!");

  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not delete in that location");
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
  } else {
    Serial.print("Unknown error: 0x");
    Serial.println(p, HEX);
  }

  return p;
}

void showSensorDetails() {
  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security level: "));
  Serial.println(finger.security_level);
  Serial.print(F("Device address: "));
  Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: "));
  Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: "));
  Serial.println(finger.baud_rate);
}

// IR Switch
void IRSwitchFunction() {
  if (IRSwitch == true) {
    int sensorValue = digitalRead(IRSensorPin);


    if (sensorValue == LOW && !IRDetected) {
      delay(50);  // Debounce delay to prevent multiple detections for one clap
      if (digitalRead(IRSensorPin) == LOW) {
        IRDetected = true;
        toggleLights();
      }
    }

    if (sensorValue == HIGH && IRDetected) {
      delay(50);  // Debounce delay to prevent multiple detections for one clap
      if (digitalRead(IRSensorPin) == HIGH) {
        IRDetected = false;
      }
    }
  }
}

//Toggle Light
void toggleLights() {
  if (!lightsOn) {
    pcf8574.digitalWrite(P1, LOW); // Turn on
    lightsOn = true;
    Serial.println("Clap ON");
  } else {
    pcf8574.digitalWrite(P1, HIGH); // Turn off
    lightsOn = false;
    Serial.println("Clap OFF");
  }
}

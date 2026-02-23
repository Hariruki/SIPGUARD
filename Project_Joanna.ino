#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- PIN DEFINITIONS ---
#define PUMP_1 2
#define PUMP_2 3
#define PUMP_3 4
#define PERI_PUMP 5
#define PH_PIN A0

// ==========================================
//    TIMING CONFIGURATION (Milliseconds)
//    Adjust these values to change durations
// ==========================================
const unsigned long T_PUMP_1     = 3000;  // Duration for Pump 1
const unsigned long T_UV_EXPOSE  = 5000;  // Duration for UV Exposure
const unsigned long T_PUMP_2     = 3000;  // Duration for Pump 2
const unsigned long T_PERI_PUMP  = 3000;  // Duration for Chlorine Pump
const unsigned long T_STABILIZE  = 5000;  // Delay for pH stabilization
const unsigned long T_PUMP_3     = 3000;  // Duration for Discharge Pump
const unsigned long T_COOLDOWN   = 10000; // Delay before restarting cycle
// ==========================================

LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long previousMillis = 0;
int currentState = 0;
float currentPH = 0.0;
bool sensorActive = false; 
String currentStatus = "Initializing";

void setup() {
  Serial.begin(9600);
  Serial.println("--- System Online ---");
  
  int pins[] = {PUMP_1, PUMP_2, PUMP_3, PERI_PUMP};
  for(int i=0; i<4; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], HIGH); // Relays OFF (Active LOW)
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
  
  delay(1500);
  previousMillis = millis();
}

void loop() {
  if (sensorActive) {
    readPH();
  }
  runStateMachine();
  updateDisplayAndSerial(); 
}

void readPH() {
  int raw = analogRead(PH_PIN);
  currentPH = raw * (14.0 / 1023.0); 
}

void runStateMachine() {
  unsigned long currentMillis = millis();

  switch (currentState) {
    case 0: // Pump 1
      currentStatus = "Pump 1 Active";
      sensorActive = false; 
      digitalWrite(PUMP_1, LOW);
      if (currentMillis - previousMillis >= T_PUMP_1) { // <--- Adjusted
        digitalWrite(PUMP_1, HIGH);
        prepareNextState(currentMillis, 1);
      }
      break;

    case 1: // UV Exposure
      currentStatus = "UV Exposed";
      if (currentMillis - previousMillis >= T_UV_EXPOSE) { // <--- Adjusted
        prepareNextState(currentMillis, 2);
      }
      break;

    case 2: // Pump 2
      currentStatus = "Pump 2 Active";
      digitalWrite(PUMP_2, LOW);
      if (currentMillis - previousMillis >= T_PUMP_2) { // <--- Adjusted
        digitalWrite(PUMP_2, HIGH);
        prepareNextState(currentMillis, 3);
      }
      break;

    case 3: // Chlorine (Peri Pump)
      currentStatus = "Add Chlorine";
      digitalWrite(PERI_PUMP, LOW);
      if (currentMillis - previousMillis >= T_PERI_PUMP) { // <--- Adjusted
        digitalWrite(PERI_PUMP, HIGH);
        sensorActive = true; 
        prepareNextState(currentMillis, 4);
      }
      break;

    case 4: // Stabilization Delay
      currentStatus = "Stabilizing...";
      if (currentMillis - previousMillis >= T_STABILIZE) { // <--- Adjusted
        prepareNextState(currentMillis, 5);
      }
      break;

    case 5: // Monitoring Phase
      currentStatus = "Monitoring pH";
      if (currentPH >= 6.5 && currentPH <= 7.5) {
        sensorActive = false;
        prepareNextState(currentMillis, 6);
      }
      break;

    case 6: // Pump 3 (Discharge)
      currentStatus = "Open Discharge";
      digitalWrite(PUMP_3, LOW); 
      if (currentMillis - previousMillis >= T_PUMP_3) { // <--- Adjusted
        digitalWrite(PUMP_3, HIGH);
        prepareNextState(currentMillis, 7);
      }
      break;

    case 7: // Cooldown
      currentStatus = "Cycle Cooldown";
      if (currentMillis - previousMillis >= T_COOLDOWN) { // <--- Adjusted
        resetCycle(currentMillis);
      }
      break;
  }
}

void prepareNextState(unsigned long now, int next) {
  previousMillis = now;
  currentState = next;
}

void resetCycle(unsigned long now) {
  previousMillis = now;
  currentState = 0;
  sensorActive = false; 
  Serial.println("--- LOG: Sequence Restarted ---");
}

void updateDisplayAndSerial() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 500) { 
    
    char line0[17];
    char line1[17];
    snprintf(line0, sizeof(line0), "S:%-14s", currentStatus.c_str());

    Serial.print("S: "); Serial.print(currentStatus);
    
    if (sensorActive) {
      String phStr = "pH: " + String(currentPH, 2);
      if (currentState == 5) {
        snprintf(line1, sizeof(line1), "%-9s WAIT ", phStr.c_str());
      } else {
        snprintf(line1, sizeof(line1), "%-16s", phStr.c_str());
      }
      Serial.print(" | pH: "); Serial.println(currentPH, 2);
    } else {
      snprintf(line1, sizeof(line1), "pH: Sensor OFF ");
      Serial.println(" | pH: Sensor OFF");
    }

    lcd.setCursor(0, 0);
    lcd.print(line0);
    lcd.setCursor(0, 1);
    lcd.print(line1);
    
    lastUpdate = millis();
  }
}

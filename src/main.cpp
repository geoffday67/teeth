/*
On start:
Flash LEDs in sequence for a few seconds.
Choose an LED number to be the special one.
Light all LEDs.

Each time there's a touch:
If the special LED has been touched then:
  Start the motor.
  Clear all LEDs.
Else:
  Clear the touched LED.
*/

#include <Arduino.h>
#include <Wire.h>

//#include <avr/interrupt.h>
//#include <avr/power.h>
#include <avr/sleep.h>

#define PIN_TOUCH_IRQ     2
#define PIN_TOUCH_RESET   12
#define PIN_MOTOR         10

volatile byte touch = 0;
byte ledState;
byte special;
bool done = false;

void clearTouch() {
  // Write 0 to bit 0 of the main control register to reset the touch detect register and interrupt
  Wire.beginTransmission(0x29);
  Wire.write(0x00);
  Wire.endTransmission();  

  Wire.requestFrom(0x29, 1);
  while (Wire.available() < 1) {
    delay(10);
  }
  byte status = Wire.read();

  Wire.beginTransmission(0x29);
  Wire.write(0x00);
  Wire.write(status & 0xFE);
  Wire.endTransmission();  
}

boolean isTouch() {
  Wire.beginTransmission(0x29);
  Wire.write(0x02);
  Wire.endTransmission();  

  Wire.requestFrom(0x29, 1);
  while (Wire.available() < 1) {
    delay(10);
  }
  byte status = Wire.read();

  return status & 0x01;
}

byte getTouch() {
  Wire.beginTransmission(0x29);
  Wire.write(0x03);
  Wire.endTransmission();  
  Wire.requestFrom(0x29, 1);
  while (Wire.available() < 1) {
    delay(10);
  }
  byte status = Wire.read();
  for (int n = 0; n < 8; n++) {
    if (status & 0x01) return n;
    status >>= 1;
  }

  // Return something, just in case.
  return 0;
}

void resetTouch() {
  digitalWrite (PIN_TOUCH_RESET, HIGH);
  delay(200);
  digitalWrite (PIN_TOUCH_RESET, LOW);
  delay(200);
}

void touchHandler() {
  //Serial.println("Touch detected");
  // Don't need to do anything here, it's enough that an interrupt happened to unblock the loop
  touch = 1;
}

void showLeds () {
  Wire.beginTransmission(0x29);
  Wire.write(0x74);
  Wire.write(ledState);
  Wire.endTransmission();  
}

void motorOn() {
  for (int n = 0; n <= 255; n += 20) {
    analogWrite(PIN_MOTOR, n);
    delay(100);
  }
}

void motorOff() {
  digitalWrite(PIN_MOTOR, LOW);
}

void startSequence() {
  byte n, m;

  for (m = 0; m < 4; m++) {
    ledState = 0x01;
    for (n = 0; n < 8; n++) {
      showLeds();
      ledState <<= 1;
      delay(50);
    }
  }

  ledState = 0xFF;
  showLeds();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting");

  randomSeed(analogRead(0));
  special = random (0, 8);
  Serial.print("Special = ");
  Serial.println(special);

  pinMode(PIN_MOTOR, OUTPUT);

  pinMode(PIN_TOUCH_IRQ, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_IRQ), touchHandler, CHANGE);

  pinMode(PIN_TOUCH_RESET, OUTPUT);
  resetTouch();

  Wire.begin();
  Wire.setClock(100000);

  // Set CAP1188 configuration
  Wire.beginTransmission(0x29);
  Wire.write(0x44);
  Wire.write(0x01);
  Wire.endTransmission();  

  // Disable auto-repeat
  Wire.beginTransmission(0x29);
  Wire.write(0x28);
  Wire.write(0x00);
  Wire.endTransmission();  

  // Configure sensitivity
  Wire.beginTransmission(0x29);
  Wire.write(0x1F);
  Wire.write(0x2F);
  Wire.endTransmission();  

  // LED ramp rates
  Wire.beginTransmission(0x29);
  Wire.write(0x94);
  Wire.write(0b00000010);
  Wire.endTransmission();  

  clearTouch();
  startSequence();
}

void loop() {
  byte touch;

  if (done) {
    return;
  }

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode(); // BLOCKS

  touch = getTouch();

  if (touch == special) {
    motorOn();
    ledState = 0;
    showLeds();
    done = true;
    return;
  }

  ledState &= ~(0x01 << touch);
  showLeds();

  // Wait for there to be no touch
  do {
    delay(50);
    clearTouch();
  } while (isTouch());
}
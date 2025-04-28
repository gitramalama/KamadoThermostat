#include "Arduino_LED_Matrix.h"

// pins
#define PB_PIN 0           // Digital pin for reading pushbutton (INPUT); yellow
#define LEDEXTERNAL_PIN 1  // Digital pin for LED (OUTPUT); orange
#define FAN_PIN 3          // PWM pin for controlling 12V fan via MOSFET (OUTPUT); blue
#define THERMISTOR_PIN A0  // Analog pin for thermistor (INPUT); green
#define POT_PIN A1         // Analog pin for potentiometer (INPUT); purple

#define SERIES_RESISTOR 10000     // 10KΩ pull-up resistor
#define NOMINAL_RESISTANCE 10000  // Resistance at 25°C (10KΩ)
#define NOMINAL_TEMPERATURE 25.0  // Nominal temperature (25°C)
#define B_COEFFICIENT 3950        // Beta coefficient (typically 3950-4000 for high-temp thermistors)

// cycle duration = the product of the following two constants
#define PULSE_INTERVAL 400  // ms
#define PULSES_PER_CYCLE 15

// instantiate matrix object
ArduinoLEDMatrix matrix;

// global variables
bool stateLEDBUILTIN;
unsigned long timeLastBlink = 0;

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);  // setup of builtin LED
}

void loop() {
  // loop variables
  bool isTimeToBlink = false;                        // flag for blinking/updating lights
  unsigned int lightsInterval = PULSE_INTERVAL * 2;  // LED blink/update rate

  // check... time to update to lights?
  if (millis() - timeLastBlink > lightsInterval) {
    // time to blink/update lights
    isTimeToBlink = true;  // set flag
  }

  // check... time to blink/update lights?
  if (isTimeToBlink) {
    // blink LED builtin
    stateLEDBUILTIN ^= 1;
    digitalWrite(LED_BUILTIN, stateLEDBUILTIN);
  }

  // check... lights flag set?
  if (isTimeToBlink) {
    // lights blink/update flag is set; reset flag and timer
    isTimeToBlink = false;
    timeLastBlink = millis();
  }
}
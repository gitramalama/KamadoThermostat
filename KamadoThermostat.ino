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

// pushbutton
#define DEBOUNCE_INTERVAL 20  // ms

// instantiate matrix object
ArduinoLEDMatrix matrix;

// global variables
bool stateLEDBUILTIN;
bool stateLEDEXTERNAL;
unsigned long timeLastBlink = 0;
bool statePBwas;
unsigned long timeLastDebounce = 0;
int pwmFan;
unsigned long frameWas[3];  // storage of LED matrix frame

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);      // setup of builtin LED
  pinMode(LEDEXTERNAL_PIN, OUTPUT);  // setup of external LED
  pinMode(PB_PIN, INPUT_PULLUP);     // setup of pushbutton
  pinMode(POT_PIN, INPUT);           // setup of potentiometer

  matrix.begin();  // setup of LED matrix
}

void loop() {
  // loop variables
  bool isTimeToBlink = false;                        // flag for blinking/updating lights
  unsigned int lightsInterval = PULSE_INTERVAL * 2;  // LED blink/update rate
  bool statePB;
  bool statePBis = digitalRead(PB_PIN);
  bool isPBpressed;
  unsigned long frame[3] = { 0, 0, 0 };
  byte fanDisplayValue;

  // debounce the pushbutton
  // check... did the pushbutton remain unchanged?
  if (statePBis == statePBwas) {
    // pushbutton remained unchanged; do nothing
  } else {
    // pushbutton changed; reset debounce timer
    timeLastDebounce = millis();
  }
  // check... has pushbutton state been the same for awhile?
  if (millis() - timeLastDebounce > DEBOUNCE_INTERVAL) {
    // pushbutton state has been the same for awhile; set pushbutton state
    statePB = statePBis;
    // check pushbutton state... is it low or high?
    if (statePB == LOW || statePB == HIGH) {
      // pushbutton state is low or high; set variable for actual pushbutton state
      stateLEDEXTERNAL = !statePB;  // opposite due to invoking input pullup resistor
      isPBpressed = !statePB;       // opposite due to invoking input pullup resistor
    }
    digitalWrite(LEDEXTERNAL_PIN, stateLEDEXTERNAL);
  }

  // potentiometer
  pwmFan = map(analogRead(POT_PIN), 0, 1023, 0, 255);
  // update LED matrix
  fanDisplayValue = pwmFan * 96.0 / 256.0;
  if (fanDisplayValue < 32) {
    frame[0] ^= (1 << (31 - fanDisplayValue % 32));
  } else if (fanDisplayValue < 64) {
    fanDisplayValue -= 32;
    frame[1] ^= (1 << (31 - fanDisplayValue % 32));
  } else {
    fanDisplayValue -= 64;
    frame[2] ^= (1 << (31 - fanDisplayValue % 32));
  }
  matrix.loadFrame(frame);

  // check... time to blink/update lights?
  if (millis() - timeLastBlink > lightsInterval) {
    // time to blink/update lights; set appropriate flag
    isTimeToBlink = true;
  }

  // check... time to blink/update lights?
  if (isTimeToBlink) {
    // variables
    // blink LED builtin
    stateLEDBUILTIN ^= 1;
    digitalWrite(LED_BUILTIN, stateLEDBUILTIN);
  }

  // reset timers and flags
  // check... lights flag set?
  if (isTimeToBlink) {
    // lights blink/update flag is set; reset timer
    timeLastBlink = millis();
  }

  // reset pushbutton debouncer
  statePBwas = statePBis;
}
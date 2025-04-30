#include "Arduino_LED_Matrix.h"

// pins
#define PB_PIN 0           // Digital pin for reading pushbutton (INPUT); yellow
#define LEDEXTERNAL_PIN 1  // Digital pin for LED (OUTPUT); orange
#define FAN_PIN 3          // PWM pin for controlling 12V fan via MOSFET (OUTPUT); blue
#define THERMISTOR_PIN A0  // Analog pin for thermistor (INPUT); green
#define POT_PIN A1         // Analog pin for potentiometer (INPUT); purple

// thermistor constants
#define SERIES_RESISTOR 10000     // 10KΩ pull-up resistor
#define NOMINAL_RESISTANCE 10000  // Resistance at 25°C (10KΩ)
#define NOMINAL_TEMPERATURE 25.0  // Nominal temperature (25°C)
#define B_COEFFICIENT 3950        // Beta coefficient (typically 3950-4000 for high-temp thermistors)

// cycle duration = the product of the following two constants
#define PULSE_INTERVAL 400  // ms
#define PULSES_PER_CYCLE 15

// pushbutton constant
#define DEBOUNCE_INTERVAL 20  // ms

// instantiate matrix object
ArduinoLEDMatrix matrix;

// global variables
bool stateLEDBUILTIN;
bool stateLEDEXTERNAL;
unsigned long timeLastPulse = 0;
unsigned long timeLastBlink = 0;
bool statePBwas;
unsigned long timeLastDebounce = 0;
int pwmFan;                            // signal to be sent to fan via MOSFET
unsigned long frameWas[3];             // storage of LED matrix frame
bool isDesiredTemperatureSet = false;  // flag for set/desired temperature established
byte nPulse = 0;                       // pulse counter
float fahr[PULSES_PER_CYCLE];          // storage of a cycle of temperature measurements
int fahrSetpoint;                      // set/desired temperature
float iPID = 0.0;                      // integral term of PID function (accumulates)
float fahrDeltaWas = 0.0;              // storage of prior cycle's temperature delta for use in PID function

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
  bool isTimeForPulse = false;                       // flag for doing pulse processing
  bool isCycleComplete = false;                      // flag for when cycle is complete
  bool isTimeToBlink = false;                        // flag for blinking/updating lights
  unsigned int lightsInterval = PULSE_INTERVAL * 2;  // LED blink/update rate
  bool statePB;                                      // flag for debouncer
  bool statePBis = digitalRead(PB_PIN);              // pushbutton input
  bool isPBpressed;                                  // flag for pushbutton ON/LATCHED
  unsigned long frame[3] = { 0, 0, 0 };              // storage LED matrix
  byte fanDisplayValue;                              // pwm signal to fan adapted to LED matrix
  float fahrDelta;                                   // temperature delta between set/desired and actual
  // lambda function for resetting PID function
  auto resetPID = []() {
    fahrDeltaWas = 0.0;
    iPID = 0.0;
  };

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
      isPBpressed = !statePB;  // opposite due to invoking input pullup resistor
    }
  }

  // check... time to blink/update lights?
  if (millis() - timeLastBlink > lightsInterval) {
    // time to blink/update lights
    isTimeToBlink = true; // flag set
    // blink builtin LED
    stateLEDBUILTIN ^= 1;
    digitalWrite(LED_BUILTIN, stateLEDBUILTIN);
  }

  // check... cycle complete?
  if (nPulse == PULSES_PER_CYCLE) {
    // cycle complete; set flag
    isCycleComplete = true;
  }

  // check... time for a pulse?
  if (millis() - timeLastPulse > PULSE_INTERVAL) {
    // time for a pulse; set flag
    isTimeForPulse = true;
  }

  // check... is pushbutton pressed?
  if (isPBpressed) {
    // pushbutton is pressed; either Auto Mode or Setting Mode

    // variables for when pushbutton is pressed
    float fahrActual;

    // do cycle processing; check... cycle complete?
    if (isCycleComplete) {
      // cycle complete; process array of thermistor inputs
      for (byte i = 0; i < PULSES_PER_CYCLE; i++) {
        for (byte j = 0; j < PULSES_PER_CYCLE - 1; j++) {
          if (fahr[j] > fahr[j + 1]) {
            float placeholder = fahr[j];
            fahr[j] = fahr[j + 1];
            fahr[j + 1] = placeholder;
          }
        }
      }
      if (PULSES_PER_CYCLE % 2 == 0) {
        fahrActual = (fahr[PULSES_PER_CYCLE / 2 - 1] + fahr[PULSES_PER_CYCLE / 2]) / 2;
      } else {
        fahrActual = fahr[PULSES_PER_CYCLE / 2];
      }
    }

    // do pulse processing; check... time for a pulse?
    if (isTimeForPulse) {
      // time for a pulse; process thermistor inputs
      int thermistorValue = analogRead(THERMISTOR_PIN);  // get thermistor input
      // calculate fahrenheit
      float steinhart = SERIES_RESISTOR / ((1023.0 / thermistorValue) - 1);
      steinhart /= NOMINAL_RESISTANCE;                    // R/Ro
      steinhart = log(steinhart);                         // ln
      steinhart /= B_COEFFICIENT;                         // 1/B * ln(R/Ro)
      steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);  // + (1/To)
      steinhart = 1.0 / steinhart;                        // Invert
      steinhart -= 273.15;                                // Convert from Kelvin to Celsius
      steinhart = (9 * steinhart / 5) + 32;               // Convert Celsius to Fahrenheit
      fahr[nPulse] = steinhart;                           // send fahrenheit value to appropriate array
      nPulse++;                                           // increment pulse counter
    }

    // check... is desired temperature set?
    if (isDesiredTemperatureSet) {
      // desired temperature is set; AUTO MODE!

      // turn on external LED
      stateLEDEXTERNAL = HIGH;
      digitalWrite(LEDEXTERNAL_PIN, stateLEDEXTERNAL);

      // check... cycle complete?
      if (isCycleComplete) {
        // cycle complete; do PID round

        // variable for auto mode
        float dPID;

        // PID function (auto mode)
        fahrDelta = fahrSetpoint - fahrActual;
        iPID += fahrDelta;
        dPID = fahrDelta - fahrDeltaWas;
        pwmFan += round(2.0 * fahrDelta + 0.08 * iPID + 10.0 * dPID);
        pwmFan = constrain(pwmFan, 0, 255);
        analogWrite(FAN_PIN, pwmFan);  // send pwm signal to 12V fan via MOSFET
        fahrDeltaWas = fahrDelta;      // store error for next cycle
        /* PID tuning
        correction = Kp * p + Ki * i + Kd * d

        Kp is the proportional gain, meaning the fan will respond directly to temperature deviations.
        If too low, the response to temperature deviations will be sluggish.
        If too high, the system will overshoot.

        Ki is the integral term's constant and will help correct steady-state errors (drift from setpoint).
        If too low, the system will be slow to respond to a drift from setpoint.
        If too high, the system will overshoot.

        Kd is the derivative term's constant and will help counteract rapid temperature changes.
        If too low, the system will anticipate poorly.
        If too high, the system may inadvertently cause a drift.

        Tuning response to system performance
        - Slow response to temperature deviation: increase Kp
        - Repeated overshoots or oscilattions: decrease Kp or Ki
        - Slow response to drift: increase Ki
        - Excessive temperature fluctuation: increase Kd
        */
      }
    } else {
      // desired temperature is not set; SETTING MODE!

      // check... is time to blink/update lights?
      if (isTimeToBlink) {
        // time to blink/update lights; blink external LED
        stateLEDEXTERNAL ^= 1;
        digitalWrite(LEDEXTERNAL_PIN, stateLEDEXTERNAL);
      }

      // fan control while establishing temperature setpoint
      pwmFan = 25;
      analogWrite(FAN_PIN, pwmFan); // send PWM value to fan via MOSFET
    }

    // check... cycle complete?
    if (isCycleComplete) {
      // cycle complete; establish setpoint and reset PID function
      fahrSetpoint = round(fahrActual);
      isDesiredTemperatureSet = true;
      resetPID();
    }
  } else {
    // pushbutton is not pressed; MANUAL MODE!

    // turn off external LED
    stateLEDEXTERNAL = LOW;
    digitalWrite(LEDEXTERNAL_PIN, stateLEDEXTERNAL);

    // process potentiometer input and send processed PWM value to fan via MOSFET
    pwmFan = map(analogRead(POT_PIN), 0, 1023, 0, 255);
    analogWrite(FAN_PIN, pwmFan);

    // various resetting associated with Manual Mode
    isDesiredTemperatureSet = false;  // reset flag
    resetPID();                       // reset PID function
    nPulse = 0;                       // reset pulse counter
  }

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
      // store matrix frame for next lights update
  for (byte i = 0; i < 3; i++) {
    frameWas[i] = frame[i];
  }

  /*********************************************
  reset timers, counters, flags and functions
  *********************************************/
  // check... cycle complete flag set?
  if (isCycleComplete) {
    // cycle complete; reset pulse counter
    nPulse = 0;
  }
  // check... pulse flag set?
  if (isTimeForPulse) {
    timeLastPulse = millis();
  }
  // check... lights flag set?
  if (isTimeToBlink) {
    // lights flag is set; reset timer
    timeLastBlink = timeLastPulse; // sync'd to pulse timer b/c lights interval is a multiple of pulse interval
  }
  // reset pushbutton debouncer
  statePBwas = statePBis;
}
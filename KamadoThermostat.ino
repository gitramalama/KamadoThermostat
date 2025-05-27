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

// fan kickstart constant
#define KICKSTART_THRESHOLD 25 // approx. 10% of fan duty cycle

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
bool isKickstartRequired;              // kickstart required when fan commanded off
unsigned long frameWas[3];             // storage of LED matrix frame
bool isDesiredTemperatureSet = false;  // flag for set/desired temperature established
byte nPulse = 0;                       // pulse counter
float fahr[PULSES_PER_CYCLE];          // storage of a cycle of temperature measurements
int fahrSetpoint;                      // set/desired temperature
float iPID = 0.0;                      // integral term of PID function (accumulates)
float fahrDelta;                       // temperature delta between set/desired and actual
float fahrDeltaWas = 0.0;              // storage of prior cycle's temperature delta for use in PID function

void setup() {
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);      // setup of builtin LED
  pinMode(LEDEXTERNAL_PIN, OUTPUT);  // setup of external LED
  pinMode(PB_PIN, INPUT_PULLUP);     // setup of pushbutton
  pinMode(POT_PIN, INPUT);           // setup of potentiometer
  pinMode(FAN_PIN, OUTPUT);          // setup of fan/MOSFET
  pinMode(THERMISTOR_PIN, INPUT);    // setup of thermistor

  matrix.begin();  // setup of LED matrix
}

void loop() {
  // loop variables
  bool isTimeForPulse = false;                       // flag for doing pulse processing
  bool isCycleComplete = false;                      // flag for doing cycle processing
  bool isTimeToBlink = false;                        // flag for blinking/updating lights
  unsigned int lightsInterval = PULSE_INTERVAL * 2;  // LED blink/update rate
  bool statePB;                                      // flag for debouncer
  bool statePBis = digitalRead(PB_PIN);              // pushbutton input
  bool isPBpressed;                                  // flag for pushbutton ON/LATCHED
  unsigned long frame[3] = { 0, 0, 0 };              // LED matrix
  // lambda function for resetting PID function
  auto resetPID = []() {
    fahrDeltaWas = 0.0;
    iPID = 0.0;
  };
  // lambda function for kickstarting fan from 0
  auto kickstartFan = [](int x) {
    if (x > KICKSTART_THRESHOLD) {
      analogWrite(FAN_PIN, 255);
      delay(PULSE_INTERVAL);
      isKickstartRequired = false;
    }
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
    isTimeToBlink = true;  // flag set
    // blink builtin LED
    stateLEDBUILTIN ^= 1;
    digitalWrite(LED_BUILTIN, stateLEDBUILTIN);
  }

  // check... cycle complete?
  if (nPulse == PULSES_PER_CYCLE) {
    // cycle complete; set flag
    isCycleComplete = true;
    nPulse = 0;
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
<<<<<<< HEAD
        pwmFan += round(1.0 * fahrDelta + 0.1 * iPID + 1.0 * dPID);
=======
        pwmFan += round(1.0 * fahrDelta + 0.2 * iPID + 1.0 * dPID);
>>>>>>> pre-kickstart
        pwmFan = constrain(pwmFan, 0, 255);
        // check... kickstart required?
        if (isKickstartRequired) {
          kickstartFan(pwmFan);
        }
        analogWrite(FAN_PIN, pwmFan);  // send pwm signal to 12V fan via MOSFET
        fahrDeltaWas = fahrDelta;      // store error for next cycle
        /* PID tuning
        correction = Kp * p + Ki * i + Kd * d

        Kp is the proportional gain, meaning the fan will respond directly to temperature deviations.
        If too low, the response to temperature deviations will be sluggish.
        If too high, the system will overshoot or overreact.

        Ki is the integral gain and will help correct steady-state errors (drift from setpoint).
        If too low, the system will be slow to respond to a drift from setpoint.
        If too high, the system will overshoot.

        Kd is the derivative gain and will help counteract rapid temperature changes.
        If too low, the system will anticipate poorly.
        If too high, the system may inadvertently cause a drift or overreact to trends.

        Tuning response to system performance
        - Slow response to significant temperature deviations: increase Kp
        - Repeat overshoots: decrease Kp or Ki OR increase Kd
        - Slow response to prolonged minor temperature deviations: increase Ki
        - Apparent self-induced drifts from setpoint: decrease Kd
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
      pwmFan = KICKSTART_THRESHOLD + 1;
      // check... kickstart required?
      if (isKickstartRequired) {
        kickstartFan(pwmFan);
      }
      analogWrite(FAN_PIN, pwmFan);  // send PWM value to fan via MOSFET
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
    // check... kickstart required?
    if (isKickstartRequired) {
      kickstartFan(pwmFan);
    }
    analogWrite(FAN_PIN, pwmFan);

    // various resetting associated with Manual Mode
    isDesiredTemperatureSet = false;  // reset flag
    resetPID();                       // reset PID function
    nPulse = 0;                       // reset pulse counter
  }

  // update LED matrix; check... is time to blink/update lights?
  if (isTimeToBlink) {
    // time to blink/update lights

    // constants & variables associated with LED matrix
    const float fahrIndex = pow(15.0, 1.0 / 6.0);  // sets full needle deflection to +/- 15 degF
    unsigned long setpointIndexOnly[2] = { 0b110000000000000000000000000, 0b11000000000000000000000 };
    unsigned long matrixOn[2];
    unsigned long matrixOff[2];
    unsigned long frame[3] = { 0, 0, 0 };
    byte fanDisplayValue;  // pwm signal to fan scaled to LED matrix

    // check... auto mode?
    if (isPBpressed && isDesiredTemperatureSet) {
      // auto mode; use top rows to indicate temperature relative to setpoint
      /* loop through finite possibilities of temperature deviation states
          00: within small tolerance of setpoint
          01: slight deviation less than setpoint
          02: modest deviation less than setpoint
          ...
          07: exceeds full deflection less than setpoint
          08: slight deviation greater than setpoint
          09: modest deviation greater than setpoint
          ...
          14: exceeds full deflection greater than setpoint
      */
      for (byte i = 0; i < 15; i++) {
        if (i == 0) {
          matrixOn[0] = 0b110000000000110000000000110;
          matrixOn[1] = setpointIndexOnly[1];
          matrixOff[0] = matrixOn[0];
          matrixOff[1] = matrixOn[1];
          if (abs(fahrDelta) < 1.0) {
            break;
          }
        }
        if (fahrDelta > 0) {
          if (i == 1) {
            matrixOn[0] = 0b110000000000100000000000100;
            matrixOff[0] = matrixOn[0];
            if (fahrDelta < pow(fahrIndex, i)) {
              break;
            }
          } else if (i < 7) {
            matrixOn[0] <<= 1;          // bit shift left one bit
            matrixOn[0] &= ~(1 << 27);  // clear 27th bit
            matrixOn[0] |= (1 << 25);   // set 25th bit
            matrixOff[0] = matrixOn[0];
            if (fahrDelta < pow(fahrIndex, i)) {
              break;
            }
          } else if (i == 7) {
            matrixOff[0] = setpointIndexOnly[0];
            if (fahrDelta >= pow(fahrIndex, i - 1)) {
              break;
            }
          }
        } else if (fahrDelta < 0) {
          if (i == 8) {
            matrixOn[0] = 0b110000000000010000000000010;
            matrixOff[0] = matrixOn[0];
            if (abs(fahrDelta) < pow(fahrIndex, i - 7)) {
              break;
            }
          } else if (i < 14) {
            matrixOn[0] >>= 1;          // bit shift right one bit
            matrixOn[0] &= ~(1 << 24);  // clear 24th bit
            matrixOn[0] |= (1 << 26);   // set 26th bit
            matrixOff[0] = matrixOn[0];
            if (i == 10) {
              matrixOn[1] = 0b10000000011000000000000000000000;
              matrixOff[1] = matrixOn[1];
            } else if (i > 10) {
              matrixOn[1] >>= 1;          // bit shift right one bit
              matrixOn[1] &= ~(1 << 20);  // clear 20th bit
              matrixOn[1] |= (1 << 22);   // set 22nd bit
              matrixOff[1] = matrixOn[1];
            }
            if (abs(fahrDelta) < pow(fahrIndex, i - 7)) {
              break;
            }
          } else if (i == 14) {
            matrixOff[0] = setpointIndexOnly[0];
            matrixOff[1] = setpointIndexOnly[1];
            if (abs(fahrDelta) >= pow(fahrIndex, i - 8)) {
              break;
            }
          }
        }
      }
    } else {
      // not in auto mode; use top rows to blink x's
      matrixOn[0] = 0b10001001000101010000101000100000;
      matrixOn[1] = 0b1000101000010101000100100010000;
      matrixOff[0] = 0;
      matrixOff[1] = 0;
    }

    // check... was frame on?
    if (frameWas[0] == matrixOn[0] && frameWas[1] == matrixOn[1]) {
      frame[0] = matrixOff[0];
      frame[1] = matrixOff[1];
    } else {
      frame[0] = matrixOn[0];
      frame[1] = matrixOn[1];
    }

    // bottom rows dedicated to fan speed indications
    fanDisplayValue = pwmFan * 12.0 / 256.0;  // fan speed scaled to LED matrix

    // build frame as a function of fan speed
    for (byte i = 0; i < fanDisplayValue; i++) {
      frame[2] ^= (1 << (23 - i));  // toggle (23-i)th bit
      frame[2] ^= (1 << (11 - i));  // toggle (11-i)th bit
    }
    if (pwmFan == 0) {
      if (frameWas[2] == 0b100000000000100000000000) {
        frame[2] = 0;
      } else {
        frame[2] = 0b100000000000100000000000;
      }
    } else if (pwmFan == 255) {
      if (frameWas[2] == 0b111111111111111111111111) {
        frame[2] = 0b111111111110111111111110;
      } else {
        frame[2] = 0b111111111111111111111111;
      }
    }

    // load the matrix for display
    matrix.loadFrame(frame);

    // store matrix frame for next lights update
    for (byte i = 0; i < 3; i++) {
      frameWas[i] = frame[i];
    }
  }

  /*********************************************
  reset timers, counters, flags and functions
  *********************************************/

  // check... will fan need a kickstart?
  if (pwmFan == 0) {
    isKickstartRequired = true;
  }

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
    timeLastBlink = timeLastPulse;  // sync'd to pulse timer b/c lights interval is a multiple of pulse interval
  }

  // reset pushbutton debouncer
  statePBwas = statePBis;
}
#include <Bounce2.h>
#include "Arduino_LED_Matrix.h"

// pins
#define PB_PIN 0           // Digital pin for reading pushbutton (INPUT); orange
#define AUTOLED_PIN 1      // Digital pin for LED (OUTPUT); yellow
#define FAN_PIN 3          // PWM pin for controlling 12V fan via MOSFET (OUTPUT); green
#define THERMISTOR_PIN A0  // Analog pin for thermistor (INPUT); blue
#define POT_PIN A1         // Analog pin for potentiometer (INPUT); purple

#define SERIES_RESISTOR 10000     // 10KΩ pull-up resistor
#define NOMINAL_RESISTANCE 10000  // Resistance at 25°C (10KΩ)
#define NOMINAL_TEMPERATURE 25.0  // Nominal temperature (25°C)
#define B_COEFFICIENT 3950        // Beta coefficient (typically 3950-4000 for high-temp thermistors)

// cycle duration = the product of the following two constants
#define PULSE_INTERVAL 400  // ms
#define PULSES_PER_CYCLE 15

// instantiate a Bounce2 object
Bounce2::Button button = Bounce2::Button();
// instantiate matrix object
ArduinoLEDMatrix matrix;

// global variables
bool isSetpointEstablished = false;
unsigned long timeLastPulse = 0;
unsigned long timeLastBlink = 0;
float fahr[PULSES_PER_CYCLE];  // storage of a cycle of temperature measurements
int fahrSetpoint;              // temperature setpoint
byte nPulse = 0;               // pulse counter within a cycle
unsigned int pwmFan;
float fahrDeltaWas = 0.0;
float iPID = 0;
bool isAUTOLEDon;
bool isLEDBUILTINon;
unsigned long frameWas[3];  // storage of LED matrix frame

void resetPID() {
  fahrDeltaWas = 0;
  iPID = 0;
}

void setup() {
  Serial.begin(9600);
  // PB setup
  button.attach(PB_PIN, INPUT_PULLUP);
  button.interval(5);           // ms
  button.setPressedState(LOW);  // low corresponds to internal pullup resistor

  pinMode(AUTOLED_PIN, OUTPUT);    // setup of auto mode LED
  pinMode(FAN_PIN, OUTPUT);        // setup of fan via MOSFET
  pinMode(THERMISTOR_PIN, INPUT);  // thermistor setup
  pinMode(POT_PIN, INPUT);         // potentiometer setup
  pinMode(LED_BUILTIN, OUTPUT);    // setup of builtin LED; blinks as a system heartbeat, independent of manual/auto modes

  // turn off auto mode LED
  isAUTOLEDon = 0;
  digitalWrite(AUTOLED_PIN, isAUTOLEDon);

  matrix.begin();  // setup of LED matrix
}

void loop() {
  // loop variables
  bool isPBpressed;                                  // flag for PB state
  bool isTimeForPulse;                               // flag for processing at regular intervals
  bool isTimeToBlink;                                // flag for blinking/updating lights
  float fahrDelta;                                   // error between setpoint and actual temperature
  unsigned int lightsInterval = PULSE_INTERVAL * 2;  // LED blink/update rate

  // check pushbutton status
  button.update();
  isPBpressed = button.isPressed();

  // check... time for a pulse?
  if (millis() - timeLastPulse > PULSE_INTERVAL) {
    // time for a pulse; set flag
    isTimeForPulse = true;  // set flag
  }

  // check... time to update to lights?
  if (millis() - timeLastBlink > lightsInterval) {
    // time to blink/update lights
    isTimeToBlink = true;  // set flag
    // blink LED builtin
    isLEDBUILTINon ^= 1;
    digitalWrite(LED_BUILTIN, isLEDBUILTINon);
  }

  // check... PB pressed?
  if (isPBpressed) {
    // PB currently pressed

    // variables for when PB is pressed
    float fahrActual;

    // check... time for a pulse?
    if (isTimeForPulse) {
      // time for a pulse...

      int thermistorValue = analogRead(THERMISTOR_PIN);  // read thermistor
      // calculate fahrenheit
      float steinhart = SERIES_RESISTOR / ((1023.0 / thermistorValue) - 1);
      steinhart /= NOMINAL_RESISTANCE;                    // R/Ro
      steinhart = log(steinhart);                         // ln
      steinhart /= B_COEFFICIENT;                         // 1/B * ln(R/Ro)
      steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);  // + (1/To)
      steinhart = 1.0 / steinhart;                        // Invert
      steinhart -= 273.15;                                // Convert from Kelvin to Celsius
      steinhart = (9 * steinhart / 5) + 32;               // Convert Celsius to Fahrenheit

      fahr[nPulse] = steinhart;  // send fahrenheit to appropriate array
      nPulse++;                  // increment pulse counter
    }

    // check... cycle complete?
    if (nPulse == PULSES_PER_CYCLE) {
      // cycle completed; determine temperature at this cycle
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

    // check... setpoint established?
    if (isSetpointEstablished) {
      // setpoint established; AUTO MODE!

      // check... time to blink/update lights?
      if (isTimeToBlink) {
        // time to blink auto mode LED
        isAUTOLEDon ^= 1;
        digitalWrite(AUTOLED_PIN, isAUTOLEDon);
      }

      // check... time for a PID round?
      if (nPulse == PULSES_PER_CYCLE) {
        // cycle complete; do a PID round

        // variables for auto mode
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

        Ki will help correct steady-state errors (drift from setpoint).
        If too low, the system will be slow to respond to a drift from setpoint.
        If too high, the system will overshoot.

        Kd will help counteract rapid temperature changes.
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
      // temperature setpoint is NOT established; establish setpoint

      // fan control while establishing temperature setpoint
      pwmFan = 25;
      analogWrite(FAN_PIN, pwmFan);

      // check... time for a cycle?
      if (nPulse == PULSES_PER_CYCLE) {
        // cycle completed; establish setpoint and initialize PID function
        fahrSetpoint = round(fahrActual);
        isSetpointEstablished = true;
        resetPID();  // reset PID variables
      }
    }
  } else {
    // PB is currently NOT pressed; MANUAL MODE!
    pwmFan = map(analogRead(POT_PIN), 0, 1023, 0, 255);
    analogWrite(FAN_PIN, pwmFan);  // send pwm signal to 12V fan via MOSFET
    isSetpointEstablished = false;
    resetPID();  // initialize PID function
    nPulse = 0;  // reset pulse counter

    // turn off auto mode LED
    isAUTOLEDon = 0;
    digitalWrite(AUTOLED_PIN, isAUTOLEDon);
  }

  // check... time to blink/update lights
  if (isTimeToBlink) {
    // time to update matrix

    // variables for matrix update
    const float fahrIndex = pow(15.0, 1.0 / 6.0);  // sets full needle deflection to +/- 15 degF
    unsigned long indicesOnly[2] = { 0b110000000000000000000000000, 0b11000000000000000000000 };
    unsigned long matrixOn[2];
    unsigned long matrixOff[2];
    unsigned long frame[3] = { 0, 0, 0 };
    byte fanIndex;

    // check... in auto mode?
    if (isPBpressed && isSetpointEstablished) {
      // auto mode; build matrix frame for temperature indications
      for (byte i = 0; i < 15; i++) {
        if (i == 0) {
          matrixOn[0] = 0b110000000000110000000000110;
          matrixOn[1] = indicesOnly[1];
          matrixOff[0] = indicesOnly[0];
          matrixOff[1] = indicesOnly[1];
          if (abs(fahrDelta) < 1.0) {
            break;
          }
        } else if (i == 1) {
          matrixOn[0] = 0b110000000000100000000000100;
          if (fahrDelta > 0 && fahrDelta < pow(fahrIndex, i)) {
            break;
          }
        } else if (i < 7) {
          matrixOn[0] <<= 1;          // bit shift left one bit
          matrixOn[0] &= ~(1 << 27);  // clears 27th bit
          matrixOn[0] |= (1 << 25);   // sets 25th bit
          if (fahrDelta > 0 && fahrDelta < pow(fahrIndex, i)) {
            break;
          }
        } else if (i == 7) {
          matrixOn[0] = 0b110000011111111111111111111;
          matrixOn[1] = 0b11110000011000000000000000000000;
          matrixOff[0] = 0b110000001111111111101111111;
          matrixOff[1] = matrixOn[1];
          if (fahrDelta >= pow(fahrIndex, i - 1)) {
            break;
          }
        } else if (i == 8) {
          matrixOn[0] = 0b110000000000010000000000010;
          matrixOn[1] = indicesOnly[1];
          matrixOff[0] = indicesOnly[0];
          matrixOff[1] = indicesOnly[1];
          if (fahrDelta < 0 && abs(fahrDelta) < pow(fahrIndex, i - 7)) {
            break;
          }
        } else if (i < 14) {
          matrixOn[0] >>= 1;          // bit shift right one bit
          matrixOn[0] &= ~(1 << 24);  // clear 24th bit
          matrixOn[0] |= (1 << 26);   // set 26th bit
          if (i == 10) {
            matrixOn[1] = 0b10000000011000000000000000000000;
          } else if (i > 10) {
            matrixOn[1] >>= 1;          // bit shift right one bit
            matrixOn[1] &= ~(1 << 20);  // clear 20th bit
            matrixOn[1] |= (1 << 22);   // set 22nd bit
          }
          if (fahrDelta < 0 && abs(fahrDelta) < pow(fahrIndex, i - 7)) {
            break;
          }
        } else if (i == 14) {
          matrixOn[0] = 0b110000011111111111111111111;
          matrixOn[1] = 0b11110000011000000000000000000000;
          matrixOff[0] = 0b110000011111111111011111111;
          matrixOff[1] = 0b11100000011000000000000000000000;
          if (fahrDelta < 0 && abs(fahrDelta) >= pow(fahrIndex, i - 8)) {
            break;
          }
        }
      }
    } else {
      // not in auto mode; make two x's
      // 31 27 24 20 18 16 11 09 05
      // 30 26 24 19 17 15 11 08 04
      matrixOn[0] = 0b10001001000101010000101000100000;
      matrixOn[1] = 0b1000101000010101000100100010000;
      matrixOff[0] = 0;
      matrixOff[1] = 0;
    }

    // check... was matrix on at same state as current state?
    if (frameWas[0] == matrixOn[0] && frameWas[1] == matrixOn[1]) {
      // matrix was on at same state; set matrix off
      for (byte i = 0; i < 2; i++) {
        frame[i] = matrixOff[i];
      }
    } else {
      // matrix was not on at same state; set matrix on
      for (byte i = 0; i < 2; i++) {
        frame[i] = matrixOn[i];
      }
    }

    // last two rows dedicated to fan speed; determine fan index
    fanIndex = pwmFan / (256.0 / 12.0);

    // build matrix frame for fan speed
    for (byte i = 0; i < fanIndex; i++) {
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

    // store matrix frame for next update
    for (byte i = 0; i < 3; i++) {
      frameWas[i] = frame[i];
    }
  }

  // reset appropriate flags, timers and counters
  // check... pulse flag set?
  if (isTimeForPulse) {
    // pulse flag set; reset flag and timer
    isTimeForPulse = false;
    timeLastPulse = millis();
  }

  // check... lights flag set?
  if (isTimeToBlink) {
    // lights blink/update flag is set; reset flag and timer
    isTimeToBlink = false;

    // considering lights interval is a multiple of pulse interval...
    timeLastBlink = timeLastPulse;  // sync lights timer with pulse timer
  }

  // check... does pulse counter need to be reset?
  if (nPulse == PULSES_PER_CYCLE) {
    // cycle complete; reset pulse counter
    nPulse = 0;
  }
}

# 🔧 Kamado Thermostat Controller

Set and maintain the temperature of a Kamado-style cooker 

---

## 📦 Features
- Temperature controlled by forced induction (12V fan via MOSFET)
- Auto mode maintains a set temperature
- Manual mode allows modification of the setpoint by manual control of the fan

---

## 🧰 Hardware

### The following hardware enables the thermostat:
- Arduino UNO R4 WiFi
- Honeywell 135-103LAG-J01 thermistor
- IRFZ44N MOSFET
- 1N4007 diode
- 10KΩ resistor
- 220Ω resistor
- WDERAIR WD1232DB 12V fan
- 2 inch ducting
- LED
- 10K potentiometer
- Latching pushbutton
- Custom manifold for attaching the 2 inch ducting to the bottom vent of the cooker

### Dpendencies
- Kamado-style cooker
- Thermometer (independent of the thermostat and thermistor). Mine is organic to my Kamado Joe cooker.

---

## 🧑‍💻 Software Dependencies
- Arduino IDE
- Libraries used (install via Library Manager):
  - 'Bounce2'
  - 'Arduino_LED_Matrix'

---

## 🚀 Getting Started

### 1. Repository: https://github.com/gitramalama/KamadoThermostat.git

### 2. Wiring
#### A. Fan and MOSFET
```
gMOSFET----------------sMOSFET---dMOSFET             12V(+)
  |                       |         |                  |
  |----[10KΩ Resistor]----|         |-[1N4007 diode||]-|
  |                       |         |                  |
[220Ω Resistor]          GND      Fan(-)             Fan(+)
  |
(appropriate PWM    
pin on microcontroller)
```

|| on the diode represents the cathode

#### B. Thermistor
```
 5V(+)
  |
[10KΩ Resistor]
  |
  |-----> (appropriate analog pin on microcontroller)
  |
[Thermistor]
  |
(GND)
```

### 3. Latching pushbutton is handled using the Bounce2 library. Relevant methods and elements:
- Instantiation as a global object
- attach()
- interval() // sets bounce lag time
- setPressedState([state]) // set to LOW b/c internal pullup resistor invoked
- update() // once per loop

### 4. LED matrix (organic to Arduino board) is handled using the Arduino_LED_Matrix library. Relevant methods and elements:
- Instantiation as a global object
- begin() // starts the matrix
- loadFrame([frame]) // loads the frame and displays it
```
  01 02 03 04 05 06 07 08 09 10 11 12
--+-----------------------------------
08|31 30 29 28 27[26 25]24 23 22 21 20
07|19 18 17 16 15[14 13]12 11 10 09 08
06|07 06 05 04 03[02 01]00+31 30 29 28
05|27 26 25 24 23[22 21]20 19 18 17 16
04|15 14 13 12 11 10 09 08 07 06 05 04
03|03 02 01 00+31 30 29 28 27 26 25 24
02|23 22 21 20 19 18 17 16 15 14 13 12
01|11 10 09 08 07 06 05 04 03 02 01 00
```
Referencing the diagram above, the matrix is a set of 12x8 LEDs. An array of three unsigned long integers were chosen to represent all the LEDs in the matrix. The first element of the array (index 0) represents the top 2-2/3 of the matrix, the second: the middle 2-2/3 and the third: the bottom 2-2/3.

The matrix indicates temperature and commanded fan speed. The top four rows of the matrix are dedicated to indicating temperature relative to the setpoint. The setpoint is represented by the middle two LEDs in the top and the fourth rows of the matrix. The relative temperature of the cooker is indicated by the second and third rows. Commanded fan speed is depicted on the bottom two rows.

Full scale deflection of the temperature represents +/- 15 degF from setpoint.

### 5. LEDs (other than the LED matrix)
#### The built-in LED indicates system heartbeat
#### External LED
- blinks ON/OFF to indicate the setpoint is being processed
- illuminates ON steady to indicate the thermostat is in AUTO mode
- is OFF when the thermostat is in MANUAL mode

# 🔧 Kamado Thermostat Controller

Set and maintain the temperature of a Kamado-style cooker 

---

## 📦 Concept of operation and features

- Concept of operation. The basic control of a Kamado cooker's oven temperature is by making manual adjustments to the top and bottom vents (natural aspiration). This thermostat uses forced induction instead. A 12VDC fan forces air in through the bottom vent to the cooker's furnace. The fan is controlled by a microcontroller via a MOSFET. The thermostat automates maintaining a desired cooking temperature and relieves the user of continually monitoring the cooker's temperature and making vent adjustments.
- Modes of operation
  - Manual Mode. This mode allows the user direct control of the fan so that the oven temperature can be adjusted to the set/desired temperature before putting the thermostat in Auto Mode. Manual Mode also enables changing the set/desired temperature while cooking. While in Manual Mode and monitoring the oven temperature, use a thermometer that is independent of the thermostat, preferably the thermometer that is organic to the cooker.
  - Auto Mode. This mode maintains the set/desired temperature.
  - Setting Mode. This mode determines the set/desired temperature.
- Mode control. The user controls the thermostat's mode of operation using the latching pushbutton.
  - OUT/UNLATCHED corresponds to Manual Mode
  - IN/LATCHED to Auto Mode
  - When the pushbutton is initially pressed to IN/LATCHED, the thermostat enters Setting Mode. Upon completion of Setting Mode (5-10 seconds), the thermostat automatically transitions to Auto Mode.
- Mode indications. The external LED indicates the mode of operation.
  - OFF steady: Manual Mode
  - ON steady: Auto Mode
  - Blinking ON/OFF: Setting Mode
- Other controls/indications
  - Manual Mode fan speed is controlled by the potentiometer
  - System heartbeat is indicated by the LED organic and built-in to the Arduino microcontroller
  - Actual temperature relative to set/desired temperature (Auto Mode only). There is a 12x8 LED matrix organic to the Arduino microcontroller. The top four rows are dedicated to indicating the actual temperature relative to the set/desired temperature. The horizontal center of the 12x8 matrix represents the set/desired temperature and the actual temperature is indicated along the second and third rows from the top. Full scale deflection is equal to +/- 15&deg;F above/below the set/desired temperature. When the actual temperature exceeds full scale deflection, the left-most or right-most LEDs will blink.
  - Fan speed. The bottom two rows of the LED matrix are dedicated to indicating commanded fan speed. If the fan is commanded OFF, the left-most LEDs will blink. If the fan is commanded to full speed the right-most LEDs will blink.

---

## Temperature limit

572&deg;F (300&deg;C)

---

## 🧰 Hardware

### The following hardware enables the thermostat:
- Arduino UNO R4 WiFi
- Honeywell 135-103LAG-J01 thermistor
- IRFZ44N MOSFET
- 1N4007 diode
- 2 x 10KΩ resistor
- 470Ω resistor
- 220&ohm; resistor
- WDERAIR WD1232DB 12V fan
- 2 inch ducting
- LED
- 10K potentiometer
- Latching pushbutton
- Custom manifold for attaching the 2 inch ducting to the bottom vent of the cooker

### Dependencies
- Kamado-style cooker
- Thermometer, independent of the thermostat and thermistor (e.g., the one organic to most Kamado-style cookers)

---

## 🧑‍💻 Software Dependencies
- Arduino IDE
- 'Arduino_LED_Matrix' library (install via Library Manager)

---

## 🚀 Getting Started

### 1. Repository
https://github.com/gitramalama/KamadoThermostat.git

### 2. Wiring
#### A. Fan and MOSFET
```
gMOSFET----------------sMOSFET---dMOSFET             12V(+)
  |                       |         |                  |
  |----[10KΩ resistor]----|         |-[1N4007 diode||]-|
  |                       |         |                  |
[470Ω resistor]          GND      Fan(-)             Fan(+)
  |
(appropriate PWM    
pin on microcontroller)
```
|| on the diode represents the cathode

#### B. Thermistor
```
 5V(+)
   |
[10KΩ resistor]
   |
   |--> (appropriate analog pin on microcontroller)
   |
[thermistor]
   |
  GND
```

#### C. Pushbutton, leveraging the microcontroller's input pullup resistor
```
(appropriate digital pin on microcontroller)
 |
[pushbutton]
 |
GND
```

#### D. External LED
```
(appropriate digital pin on microcontroller)
    |
[220Ω resistor]
    |
 -------
| anode |
|  LED  |
|cathode|
 -------
    |
   GND
```

### 3. LED matrix (organic to Arduino board) is handled using the Arduino_LED_Matrix library. Relevant methods and elements:
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

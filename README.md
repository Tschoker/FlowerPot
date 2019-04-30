# FlowerPot
Automatic watering system with water level and moisture sensor including MQTT integration.
This project currently only exists on the breadboard. Missing material, tools and time I wasn't able to construct the actual tank yet - to be updated hopefully soon.

# Idea
In principle this will be a water tank holding the water for the flowers (in my case Orchids - therefor the case has some special requirements) that sits below the actual flower pot. On a regular basis, and controlled with moisture measurement, some water from the tank will be pumped to the flower pot - excess running through the soil and dripping back into the tank.
Additionally the water level in the tank will be measured and a LED will indicate current status. Also MQTT integration is done.

# Features
* Water Level measurement
* Deactivating pump at water low level
* MQTT notification about water level, moisture level and pump status
* LED indicating water level (OK=off, warning=ON, alert=BLINKING)
* moisture sensor to start watering when level is too low (regardless of time schedule)

# Hardware
Here I only list whats technically needed, as the construction of the tank itself can be done many various ways:
* MCU - I used a WeMos D1 mini
* UltraSonic distance sensor (HC-SR04)
* LED (3mm green + suiting resistor)
* Mini Waterpump (https://www.amazon.de/dp/B07F9MT92K/ref=cm_sw_em_r_mt_dp_U_owdYCbF7MC67Q)
* 1N4001 diode set in parallel of the pump, but in reverse polarity to kill any induction current when turning off the pump.
* BC337 Transistor to switch on/off the pump
* 1k resistor between data pin of MCU and base of transistor
* Inductive moisture sensor (https://www.amazon.de/dp/B07MK5MPSN/ref=cm_sw_em_r_mt_dp_U_DydYCb307QESC)
* some dupont conenctors and wires
* PVC hose (tube)
* Filter material to prevent dirt from entering the pump (either directly where the water drips back into the tank, or just around the pump itself

# Schematic
coming soon

Basic explanation:
The LED and SR04 are connected to the data pins as usual (dont forget a suiting resistor for the LED). The moisture sensor will be connected to the ADC on A0.
About the pump: The data pin of the MCU will be connected through a ~1k resistor with the base of the transistor. The Emitter will be set to GND. The Collector will be connected to the negative wire of the pump. The positive wire will be connected to the +5V of MCU - no worries, on D1 mini there is no voltage regulator there, just a diode and a fuse. For other MCU please check if the power source can drive the pump you have.
In parallel to the pump, so between +5V and Collector, you have to (should) place a diode in reverse direction (catode in the direction of +5V). This is needed as the pump (or any other motor) will still rotate when you switch it off. Normal motors consisting af a magnet and a coil will then start inducting current into the system - sometimes very much indeed. To drain this current we use the diode. Otherwise you could damage your transistor or even MCU. If you test it without a diode it will most probably also work without any damage, but especially semiconductors just don't like these kind of induction current. So after some time some components could get damaged. It is just good practice to place a relieve diode there.

# Images
not yet build

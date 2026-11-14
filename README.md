# ML-KEM vs RSA temperature fingerprint

## Zones
1. Substrate from the right of CPU
2. CPU silicon package
3. Substrate below CPU
4. Micro USB
5. LDO U6
6. LDO U5
7. Substrate from the left of CPU

---

## Time of Experiments
- 20.02-20.10 substrate right (z1), CPU (z2)
- 20.19 substrate below CPU (z3)
- 20.50 D8 micro usb (z4), d7 ldo (z5)
- 20.04 D8 micro usb (z4), d7 ldo (z5) ml-kem +  21.17 D8 micro usb (z4), d7 ldo (z5) rsa
- 21.51 D8 LDO U5(z6), D7 Substrate left cpu (z7) kem + 22.07 D8 LDO U5(z6), D7 Substrate left cpu (z7) rsa
- 22.27 idle z2 cpu + z7 substrate left

8 minutes each zone

---

## Temperature Sensors Setup
Use 4k7 resistor.
Don't use 47 Ohm resistor because it may damage GPIO pin.

Look at sensor DS18B20 flat face:
1. pin 1 GND => use dupont wire to connect to board GND
2. pin 2 Data DQ => add resistor 4k7 leg 1 => use dupont wire to connect to board pin to transmit temperature
3. pin 3 VDD => add resistor 4k7 leg 2 => use dupont wire to connect to board VDD pin

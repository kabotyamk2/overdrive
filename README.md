# OVERDRIVE

Teensy 4.0 RC/LED Controller

![](./teensy40_overdrive_beta1/wire1.jpg)

## Requirement
* Teensy 4.0
* 4ch rc receiver/transmitter (test with Futaba R334SBS-E/Futaba 7PX)
* Jetson Nano (I don't test on raspberry pi3/4.)

## Known issue
* sometime i2c error occures with PCA9685 emulator.<br>
* Some motors seem to be affected by noise. (maybe tired motor?)<br>
  * TT-02 27T normal motor: no effect. good condition.<br>
  * YM-SS58: It seems to be greatly affected by motor noise.<br>
When I put the Teensy before the front bumper, there is no problem up to 50% throttle, but the interrupt pulse check will be distorted at 100% throttle.<br>
When I put the Teensy in the center of the rc car, the interrupt pulse check will be distorted at 25% throttle.<br>

## Teensy Setup
[Arduino IDE](https://www.arduino.cc/en/main/software)<br>
[Teensyduino](https://www.pjrc.com/teensy/td_download.html)<br>
[teensy4_i2c](https://github.com/Richard-Gemmell/teensy4_i2c)<br>
* Install Arduino IDE.
* Install Teensyduino.
* Install teensy4_i2c.
* Setup Arduino IDE

### Install Arduino IDE.
Download ARDUINO 1.8.12 Windows ZIP file for non admin install.<br>
unzip it.<br>

### Install Teensyduino.
Download Teensyduino 1.51 Windows XP/7/8/10 Installer.<br>
execute and install.<br>

### Install teensy4_i2c.
git clone and copy the directory into arduino libraries.<br>
```
git clone https://github.com/Richard-Gemmell/teensy4_i2c
cp -r teensy4_i2c arduino/hardware/teensy/avr/libraries/
```

## Setup Arduino IDE
* Tools
Board: "Teensy 4.0"<br>
USB Type: "Serial + Keyboard + Mouse + Joystick"<br>
CPU Speed: "600 MHz"<br>
Optimize: "Faster"<br>
![](./teensyduino.png)

## DonkeyCar Setup
Use donkeycar 3.1.1.<br>
```
cp donkeycar311/*.py ~/project/donkeycar/donkeycar/parts/
cp donkeycar311/myconfig.py.nano_120fps ~/mycar/myconfig.py
```

## Transmitter Settings
Steering and throttle: These are normal rc car setting.<br>
* ch1
  * steering
* ch2
  * throttle
* ch3
  * manual - auto mode change.
* ch4
  * delete record.

![](./transmitter.jpg)<br>
![](./transmitter_manual.jpg)<br>
![](./transmitter_auto.jpg)<br>

## Record training data
* Transmitter CH3 auto mode.
* Transmitter CH3 manual mode. (change from auto to manual mode is one of flags)
* Transmitter CH2 throttle on. (start recording)
* Transmitter CH3 auto mode. (stop recording)
* Transmitter CH4 delete records. (delete 120 records.)

## Autonomous driving
* Transmitter CH3 auto mode.<br>
If you think your rc car will crash, apply the brakes immediately. Overdrive gives priority to manual operation.<br>
One second after you release your hand, it switches to automatic mode.<br>

## Teensy 4.0 OVERDRIVE beta1
See [Teensy 4.0 OVERDRIVE beta1](./README_teensy40_overdrive_beta1.md)<br>


# Teensy 4.0 OVERDRIVE beta1

## Breakout Board
![](./teensy40_overdrive_beta1/teensy40_overdrive_beta1.png)<br>

## Code
[teensy40_overdrive_beta1/teensy40_overdrive_beta1.ino](./teensy40_overdrive_beta1/teensy40_overdrive_beta1.ino)

## Wiring
In these pictures, LED soldering is not yet complete.<br>
![](./teensy40_overdrive_beta1/wire1.jpg)<br>
![](./teensy40_overdrive_beta1/wire2.jpg)<br>
![](./teensy40_overdrive_beta1/wire3.jpg)<br>
![](./teensy40_overdrive_beta1/reverse.jpg)<br>

## Tuning
Using an oscilloscope, enter the time (in microseconds) during which the voltage is HIGH.<br>
<b>Important: MIN is minimum pulse. You can forget which is left, right, forward, brake.</b>

```
/* STEERING PULSE */
const int RECV_CH1_PULSE_LENGTH_MIN     = 1240; // maximum steering right value
const int NEUTRAL_STEERING_PULSE_LENGTH = 1520; // neutral steering value
const int RECV_CH1_PULSE_LENGTH_MAX     = 1740; // maximum steering left value

/* THROTTLE PULSE */
const int RECV_CH2_PULSE_LENGTH_MIN     = 1080; // maximum throttle forward value
const int NEUTRAL_THROTTLE_PULSE_LENGTH = 1520; // neutral throttle value
const int RECV_CH2_PULSE_LENGTH_MAX     = 1980; // maximum throttle brake value
```
If you use reversed throttle, then set REVERSE 1. (default 0)
```
#define REVERSE 1              // TS-50A ESC should be 1. This uses only for led controll.
```
If you want to use PCA9685 board, then set USE_PCA9685_EMULATOR 0 and use P1/P2 pin. (default 1)
```
#define USE_PCA9685_EMULATOR 0
```
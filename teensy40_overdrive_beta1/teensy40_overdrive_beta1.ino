/*
 * MIT License
 *
 * Copyright (c) 2019 naisy
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *SOFTWARE.
 */

/*
 * Requirements
 * Teensy 4.0
 * teensy4_i2c library: https://github.com/Richard-Gemmell/teensy4_i2c
 *   git clone https://github.com/Richard-Gemmell/teensy4_i2c
 *   cp -r teensy4_i2c arduino/hardware/teensy/avr/libraries/
 * 4ch RC Transmitter: Futaba 7PX, Futaba 4PM, etc.
 *   3ch RC Transmitter will be work.
 */
#include "TeensyThreads.h"

#define len(x)  int(sizeof(x) / sizeof((x)[0]))
// FOR TT-02 LaFerrari
const int RECV_CH1_PULSE_LENGTH_MIN     = 1240; // maximum steering right value
const int NEUTRAL_STEERING_PULSE_LENGTH = 1520; // neutral steering value
const int RECV_CH1_PULSE_LENGTH_MAX     = 1740; // maximum steering left value
const int RECV_CH2_PULSE_LENGTH_MIN     = 1080; // maximum throttle forward value
const int NEUTRAL_THROTTLE_PULSE_LENGTH = 1520; // neutral throttle value
const int RECV_CH2_PULSE_LENGTH_MAX     = 1980; // maximum throttle brake value
// FOR TT-02 RR GeForce TS-50A ESC
/*
const int RECV_CH1_PULSE_LENGTH_MIN     = 1240; // maximum steering right value
const int NEUTRAL_STEERING_PULSE_LENGTH = 1520; // neutral steering value
const int RECV_CH1_PULSE_LENGTH_MAX     = 1720; // maximum steering left value
const int RECV_CH2_PULSE_LENGTH_MIN     = 1040; // maximum throttle brake value
const int NEUTRAL_THROTTLE_PULSE_LENGTH = 1520; // neutral throttle value
const int RECV_CH2_PULSE_LENGTH_MAX     = 2000; // maximum throttle forward value
*/
#define DEBUG 0
#define USE_SYSTEM_PING 0
#define USE_JOYSTICK 1         // /dev/input/js1
#define REVERSE 0              // TS-50A ESC should be 1. This uses only for led controll.
#define USE_PCA9685_EMULATOR 1

/*
 * Threshold for receiver priority.
 * Used to prevent crashes in auto-driving mode.
 */
const int STEERING_PULSE_LENGTH_MIN_THRESHOLD  = NEUTRAL_STEERING_PULSE_LENGTH - (NEUTRAL_STEERING_PULSE_LENGTH - RECV_CH1_PULSE_LENGTH_MIN)/10;
const int STEERING_PULSE_LENGTH_MAX_THRESHOLD  = NEUTRAL_STEERING_PULSE_LENGTH - (NEUTRAL_STEERING_PULSE_LENGTH - RECV_CH1_PULSE_LENGTH_MAX)/10;
const int THROTTLE_PULSE_LENGTH_MIN_THRESHOLD  = NEUTRAL_THROTTLE_PULSE_LENGTH - (NEUTRAL_THROTTLE_PULSE_LENGTH - RECV_CH2_PULSE_LENGTH_MIN)/10;
const int THROTTLE_PULSE_LENGTH_MAX_THRESHOLD  = NEUTRAL_THROTTLE_PULSE_LENGTH - (NEUTRAL_THROTTLE_PULSE_LENGTH - RECV_CH2_PULSE_LENGTH_MAX)/10;

/* 
 * PROCESSING HZ
 * LOOP_HZ: main loop Hz
 * PWM_OUT_HZ: PWM output Hz
 * LED_HZ: LED processing Hz
 */
const int LOOP_HZ                  = 1200; // main loop hz
const int PWM_OUT_HZ               = 60;   // joystick output hz
const int LED_HZ                   = 100;
const int LED_BLINK_HZ             = 200;
const int LED_BLINK2_HZ            = 400;
const int LED_FLUC_HZ              = 255;
const unsigned long LOOP_INTERVAL  = long(1000)*long(1000)/long(LOOP_HZ);
unsigned long hz_counter           = 0;
unsigned long micros_interval      = 16000;
unsigned long micros_slept         = 16000;

/*
 * SPEED STATUS for LED
 */
const int SPEED_BRAKE         = -1;
const int SPEED_NEUTRAL       = 0;
const int SPEED_MIDDLE        = 1;
const int SPEED_TOP           = 2;
int speed_status              = SPEED_NEUTRAL;
/*
const int recv_speed_up_range[]    = {1540, 1440, 1240}; // greater than: NEUTRAL, MIDDLE, TOP
const int recv_speed_down_range[]  = {1580, 1480, 1280}; // less than: BRAKE, NEUTRAL, MIDDLE
*/
#if !REVERSE
  const int speed_up[]          = {+16, -84, -284}; // greater than: NEUTRAL, MIDDLE, TOP
  const int speed_down[]        = {+56, -44, -244}; // greater than: NEUTRAL, MIDDLE, TOP
#else
  const int speed_up[]          = {-16, +84, +284}; // greater than: NEUTRAL, MIDDLE, TOP
  const int speed_down[]        = {-56, +44, +244}; // greater than: NEUTRAL, MIDDLE, TOP
#endif
const int speed_up_range[]      = {NEUTRAL_STEERING_PULSE_LENGTH+speed_up[0], NEUTRAL_STEERING_PULSE_LENGTH+speed_up[1], NEUTRAL_STEERING_PULSE_LENGTH+speed_up[2]}; // greater than: NEUTRAL, MIDDLE, TOP
const int speed_down_range[]    = {NEUTRAL_STEERING_PULSE_LENGTH+speed_down[0], NEUTRAL_STEERING_PULSE_LENGTH+speed_down[1], NEUTRAL_STEERING_PULSE_LENGTH+speed_down[2]}; // greater than: NEUTRAL, MIDDLE, TOP

/*
 * INPUT PIN
 * RECV_CH1: 22     - Steering
 * RECV_CH2: 21     - Throttle
 * RECV_CH3: 20     - Manual/Auto mode
 * RECV_CH4: 17     - Delete 100 records
 * PCA9685_CH1: 12  - Steering
 * PCA9685_CH2: 11  - Throttle
 * SYSTEM_CH1: 13   - System dead or alive check
 */
const byte INPUT_PIN[]  = {22, 21, 20, 17, 12, 11, 13};
const byte RECV_CH1     = 0; // index of array, exInterrupt
const byte RECV_CH2     = 1; // index of array, exInterrupt
const byte RECV_CH3     = 2; // index of array, Interrupt
const byte RECV_CH4     = 3; // index of array, Interrupt
const byte PCA9685_CH1  = 4; // index of array, exInterrupt
const byte PCA9685_CH2  = 5; // index of array, exInterrupt
const byte SYSTEM_CH1   = 6; // index of array, exInterrupt, with no pulse check

/* 
 * micros_last[RECV_CH1]
 * micros_last[RECV_CH2]
 * micros_last[RECV_CH3]
 * micros_last[RECV_CH4]
 * micros_last[PCA9685_CH1]
 * micros_last[PCA9685_CH2]
 * micros_last[SYSTEM_CH1]
 * micros_last[CURRENT_CH1]
 * micros_last[DELTA_TIME]
 * micros_last[WAKEUP_TIME]
 * micros_last[FORCE_TIME]
 */
volatile unsigned long micros_last[]   = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
const byte CURRENT                     = 7; // index of array
const byte DELTA_TIME                  = 8; // index of array
const byte WAKEUP_TIME                 = 9; // index of array
const byte FORCE_TIME                  = 10; // index of array
volatile unsigned long micros_ch1      = 0;
volatile unsigned long micros_ch2      = 0;
volatile unsigned long rising_start[]  = {0, 0, 0, 0, 0, 0};
volatile long input_pulse_length[]     = {0, 0, 0, 0, 0, 0};
volatile int high_low[]                = {0, 0, 0, 0, 0, 0}; // HIGH or LOW

/*
 * STATUS
 */
const bool PCA9685   = true;  // MODE PCA9685
const bool RECEIVER  = false; // MODE RECEIVER
const bool ALIVE     = true;  // PING SYSTEM ALIVE
const bool DEAD      = false; // PING SYSTEM DEAD
const bool DELETE    = true;  // DELETE ON
const bool FORCE     = true;  // FORCE RECEIVER MODE
const bool PUSHED    = true;  // BUTTON PUSHED STATUS

/*
 * status[ST_MODE]:
 *   RECEIVER: receiver mode
 *   PCA9685: pca9685 mode
 * status[ST_DELETE]:
 *   !DELETE: nothing
 *   DELETE: delete count++. (delete 100 records)
 * status[ST_PING]:
 *   DEAD: system dead
 *   ALIVE: system alive
 * status[ST_FORCE_RECEIVER]:
 *   !FORCE: normal mode
 *   FORCE: force receiver mode
 * status[ST_MANUAL_STEERING]:
 *   !FORCE: normal mode
 *   FORCE: force receiver mode
 * status[ST_MANUAL_THROTTLE]:
 *   !FORCE: normal mode
 *   FORCE: force receiver mode
 *
 * button_status[BT_MODE]:
 *   same as status[ST_MODE]
 *   !PUSHED: release, == RECEIVER
 *   PUSHED: push, == PCA9685
 * button_status[BT_DELETE]:
 *   !PUSHED: release,  == !DELETE
 *   PUSHED: push, == DELETE
 * delete_counter:
 *   count how many delete button pushed
 *
 * signal_alive[RECV_CH1]:
 *   DEAD:  receiver no signal.
 *   ALIVE: receiver with signal.
 * signal_alive[RECV_CH2]:
 *   Not used.
 * signal_alive[RECV_CH3]:
 *   Not used.
 * signal_alive[RECV_CH4]:
 *   Not used.
 * signal_alive[PCA9685_CH1]:
 *   DEAD:  pca9685 no signal.
 *   ALIVE: pca9685 with signal.
 * signal_alive[PCA9685_CH2]:
 *   Not used.
 * signal_alive[SYSTEM_CH1]:
 *   Not used. Used as status[ST_PING].
 */
#if USE_SYSTEM_PING
  volatile byte status[]         = {RECEIVER, !DELETE, DEAD, !FORCE, !FORCE, !FORCE};
#else
  volatile byte status[]         = {RECEIVER, !DELETE, ALIVE, !FORCE, !FORCE, !FORCE};
#endif
volatile bool button_status[]  = {!PUSHED, !PUSHED};
volatile int delete_counter    = 0;
const int ST_MODE              = 0; // index of array
const int ST_DELETE            = 1; // index of array
const int ST_PING              = 2; // index of array
const int ST_FORCE_RECEIVER    = 3; // index of array
const int ST_MANUAL_STEERING   = 4; // index of array
const int ST_MANUAL_THROTTLE   = 5; // index of array
const int BT_MODE              = 0; // index of array
const int BT_DELETE            = 1; // index of array
bool signal_alive[]            = {DEAD, DEAD, DEAD, DEAD, DEAD, DEAD, DEAD};

/*
 * PWM OUTPUT PIN
 * SERVO: 16
 * ESC: 15
 */
const int PWM_OUTPUT_PIN[] = {16, 15};
const int OUTPUT_CH1 = 0; // array index of PWM_OUTPUT_PIN
const int OUTPUT_CH2 = 1; // array index of PWM_OUTPUT_PIN

/*
 * LED OUTPUT PIN
 * SYSTEM LED: 0
 * HEAD LIGHT: 1
 * BRAKE LIGHT: 2
 * OTHER: 3
 */
const int LED_PIN[]  = {0, 1, 2, 3};
const int NUM_LEDS   = len(LED_PIN);

/* LED_CONFIG:
 * PIN, PATTERN, HZ, MIN, MAX, CURRENT, INCREMENT, CURRENT_INCREMENT, 
 * 8:BLINK_ON_TIMES, BLINK_ON_HZ_LENGTH, BLINK_OFF_HZ_LENGTH, BLINK_CURRENT_HZ, BLINK_ON_COUNT, BLINK_CURRENT_STATUS,
 * 14:BB_LENGTH, BB_ON_TIMES, BB_ON_HZ_LENGTH, BB_OFF_HZ_LENGTH, BB_GLOBAL_HZ, BB_ON_COUNT, BB_CURRENT_STATUS, BB_LOCAL_HZ
 */
const int LED_POWER_MIN = 0;
const int LED_POWER_1 = 1;
const int LED_POWER_2 = 8;
const int LED_POWER_3 = 16;
const int LED_POWER_4 = 32;
const int LED_POWER_MAX = 255;
const int LED_OFF = 0; // LED: 0
const int LED_ON = 1; // LED: 255
const int LED_FLUCTUATION = 2; // LED: 0 to 255
const int LED_BLINK = 3; // LED: 0 or 255
const int LED_BLINK2 = 5; // LED: 0 or 255
const int LED_BLINK_X_BLINK = 4; // LED: 0 or 255
const int LED_CONFIG[] = {3,  LED_OFF, LED_HZ, LED_POWER_MIN, LED_POWER_MAX, 0, 1, 1, 2, 1, 9, 0, 0, 0, 6, 4, 1, 0, 0, 0, 0, 0}; // initial value
int led_configs[NUM_LEDS][len(LED_CONFIG)] = {
  {0, LED_OFF, LED_HZ, LED_POWER_MIN, LED_POWER_MAX, 0, 1, 1, 2, 1, 9, 0, 0, 0, 6, 4, 1, 0, 0, 0, 0, 0},
  {1, LED_OFF, LED_HZ, LED_POWER_MIN, LED_POWER_MAX, 0, 1, 1, 2, 1, 9, 0, 0, 0, 6, 4, 1, 0, 0, 0, 0, 0},
  {2, LED_OFF, LED_HZ, LED_POWER_MIN, LED_POWER_MAX, 0, 1, 1, 2, 1, 9, 0, 0, 0, 6, 4, 1, 0, 0, 0, 0, 0},
  {3,  LED_OFF, LED_HZ, LED_POWER_MIN, LED_POWER_MAX, 0, 1, 1, 2, 1, 9, 0, 0, 0, 6, 4, 1, 0, 0, 0, 0, 0}
};
const int SYSTEM_LIGHT  = 0;
const int HEAD_LIGHT    = 1;
const int BRAKE_LIGHT   = 2;
const int OTHER_LIGHT   = 3;

int next_value = 0;
int increment = 0;


#if USE_PCA9685_EMULATOR
#include "PCA9685Emulator.h"
const int PCA9685_I2C_ADDRESS = 0x40;
const int PCA9685_HZ          = 60;
const unsigned long PCA9685_INTERVAL = long(1000)*long(1000)/long(PCA9685_HZ);
PCA9685Emulator pwmEmulation;

void pca9685_emulator_ch1_thread() {
  uint16_t pulse_ch1 = 0;
  while(1) {
    if (status[ST_MODE] == PCA9685 && status[ST_PING] == ALIVE && status[ST_FORCE_RECEIVER] != FORCE) {
      pulse_ch1 = pwmEmulation.readChannelUs(OUTPUT_CH1);
      if (600 <= pulse_ch1 && pulse_ch1 <= 2600) {
          micros_last[PCA9685_CH1] = micros();
          input_pulse_length[PCA9685_CH1] = pulse_ch1;
          digitalWriteFast(PWM_OUTPUT_PIN[OUTPUT_CH1], HIGH);
      }
    }
    delayMicroseconds(pulse_ch1);
    if (status[ST_MODE] == PCA9685 && status[ST_PING] == ALIVE && status[ST_FORCE_RECEIVER] != FORCE) {
      digitalWriteFast(PWM_OUTPUT_PIN[OUTPUT_CH1], LOW);
    }
    delayMicroseconds(PCA9685_INTERVAL - pulse_ch1);
  }
}

void pca9685_emulator_ch2_thread() {
  uint16_t pulse_ch2 = 0;
  while(1) {
    if (status[ST_MODE] == PCA9685 && status[ST_PING] == ALIVE && status[ST_FORCE_RECEIVER] != FORCE) {
      pulse_ch2 = pwmEmulation.readChannelUs(OUTPUT_CH2);
      if (600 <= pulse_ch2 && pulse_ch2 <= 2600) {
          micros_last[PCA9685_CH2] = micros();
          input_pulse_length[PCA9685_CH2] = pulse_ch2;
          digitalWriteFast(PWM_OUTPUT_PIN[OUTPUT_CH2], HIGH);
      }
    }
    delayMicroseconds(pulse_ch2);
    if (status[ST_MODE] == PCA9685 && status[ST_PING] == ALIVE && status[ST_FORCE_RECEIVER] != FORCE) {
      digitalWriteFast(PWM_OUTPUT_PIN[OUTPUT_CH2], LOW);
    }
    delayMicroseconds(PCA9685_INTERVAL - pulse_ch2);
  }
}
#endif


void setup()
{
  Serial.begin(57600);

 // PIN MODE: INPUT
  pinMode(INPUT_PIN[RECV_CH1], INPUT);
  pinMode(INPUT_PIN[RECV_CH2], INPUT);
  pinMode(INPUT_PIN[RECV_CH3], INPUT);
  pinMode(INPUT_PIN[RECV_CH4], INPUT);
  pinMode(INPUT_PIN[PCA9685_CH1], INPUT);
  pinMode(INPUT_PIN[PCA9685_CH2], INPUT);
#if USE_SYSTEM_PING
  pinMode(INPUT_PIN[SYSTEM_CH1], INPUT);
#endif

  attachInterrupt(digitalPinToInterrupt(INPUT_PIN[RECV_CH1]), onSignalChanged1, CHANGE); // RECEIVER
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN[RECV_CH2]), onSignalChanged2, CHANGE); // RECEIVER
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN[RECV_CH3]), onSignalChanged3, CHANGE); // ST_MODE
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN[RECV_CH4]), onSignalChanged4, CHANGE); // ST_DELETE
#if !USE_PCA9685_EMULATOR
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN[PCA9685_CH1]), onSignalChanged5, CHANGE); // PCA9685
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN[PCA9685_CH2]), onSignalChanged6, CHANGE); // PCA9685
#endif
#if USE_SYSTEM_PING
  attachInterrupt(digitalPinToInterrupt(INPUT_PIN[SYSTEM_CH1]), onSignalChanged7, CHANGE); // ST_PING
#endif

  for(int i=0; i<len(LED_PIN); i++) {
    pinMode(LED_PIN[i], OUTPUT);
  }
  pinMode(PWM_OUTPUT_PIN[OUTPUT_CH1], OUTPUT);
  pinMode(PWM_OUTPUT_PIN[OUTPUT_CH2], OUTPUT);
  

#if USE_JOYSTICK
  /* Initialize Joystick */
  Joystick.button(1, 0);
  Joystick.button(2, 0);

  Joystick.X(512); // "value" is from 0 to 1023. 512 is resting position
  Joystick.Y(512);
#endif

#if USE_PCA9685_EMULATOR
  pwmEmulation.begin(PCA9685_I2C_ADDRESS);
  threads.addThread(pca9685_emulator_ch1_thread);
  threads.addThread(pca9685_emulator_ch2_thread);
#endif

  micros_last[CURRENT] = micros();

}

int readPulse(byte index)
{
  micros_last[index] = micros();
  if (digitalRead(INPUT_PIN[index]) == HIGH) { /* start pulse length measurement */
    rising_start[index] = micros();
    return HIGH;
  } else { /* end pulse length measurement */
    input_pulse_length[index] = micros() - rising_start[index];
    return LOW;
  }
}

void onSignalChanged1(void)
{
  /* RECV_CH1 */
  high_low[RECV_CH1] = readPulse(RECV_CH1);
  if (high_low[RECV_CH1] == LOW) {
    /* FORCE RECEVER MODE THRESHOLD CHECK */
    if (input_pulse_length[RECV_CH1] <= STEERING_PULSE_LENGTH_MIN_THRESHOLD ||
        STEERING_PULSE_LENGTH_MAX_THRESHOLD <= input_pulse_length[RECV_CH1]) {
      status[ST_MANUAL_STEERING] = FORCE;
      micros_last[FORCE_TIME] = micros();
    } else {
      micros_ch1 = micros();
      if (micros_ch1 - micros_last[FORCE_TIME] >= 1000000 && micros_ch1 > micros_last[FORCE_TIME]) { // more than 1 sec
        status[ST_MANUAL_STEERING] = !FORCE;
      }
    }
    if (status[ST_MANUAL_STEERING] || status[ST_MANUAL_THROTTLE]) {
      status[ST_FORCE_RECEIVER] = FORCE;
    } else {
      status[ST_FORCE_RECEIVER] = !FORCE;
    }
  }
  /* if ST_MODE == RECEIVER or FORCE_RECEIVER */
  if (status[ST_MODE] == RECEIVER || status[ST_FORCE_RECEIVER] == FORCE) {
    digitalWriteFast(PWM_OUTPUT_PIN[OUTPUT_CH1], high_low[RECV_CH1]);
  }
}

void onSignalChanged2(void)
{
  /* RECV_CH2 */
  high_low[RECV_CH2] = readPulse(RECV_CH2);
  if (high_low[RECV_CH2] == LOW) {
    /* FORCE RECEVER MODE THRESHOLD CHECK */
    if (input_pulse_length[RECV_CH2] <= THROTTLE_PULSE_LENGTH_MIN_THRESHOLD ||
        THROTTLE_PULSE_LENGTH_MAX_THRESHOLD <= input_pulse_length[RECV_CH2]) {
      status[ST_MANUAL_THROTTLE] = FORCE;
      micros_last[FORCE_TIME] = micros();
    } else {
      micros_ch2 = micros();
      if (micros_ch2 - micros_last[FORCE_TIME] >= 1000000 && micros_ch2 > micros_last[FORCE_TIME]) { // more than 1 sec
        status[ST_MANUAL_THROTTLE] = !FORCE;
      }
    }
    if (status[ST_MANUAL_THROTTLE] || status[ST_MANUAL_STEERING]) {
      status[ST_FORCE_RECEIVER] = FORCE;
    } else {
      status[ST_FORCE_RECEIVER] = !FORCE;
    }
  }

  /* if ST_MODE == RECEIVER or FORCE_RECEIVER */
  if (status[ST_MODE] == RECEIVER || status[ST_FORCE_RECEIVER] == FORCE) {
    digitalWriteFast(PWM_OUTPUT_PIN[OUTPUT_CH2], high_low[RECV_CH2]);
  }
}

void onSignalChanged3(void)
/*
 * ST_MODE
 */
{
  /* RECV_CH3 */
  high_low[RECV_CH3] = readPulse(RECV_CH3);
  if (high_low[RECV_CH3] == HIGH) {
    return;
  }
  if (input_pulse_length[RECV_CH3] < 1500) {
    /* button push == false */
    /* will be receiver mode */
    status[ST_MODE] = RECEIVER;
    button_status[BT_MODE] = RECEIVER;
    set_led_blink(OTHER_LIGHT, LED_POWER_MAX, false);
  } else {
    /* button push == true */
    /* will be pca9685 mode */
    status[ST_MODE] = PCA9685;
    button_status[BT_MODE] = PCA9685;
    set_led_on(OTHER_LIGHT, LED_POWER_3, false);
  }
}

void onSignalChanged4(void)
/*
 * ST_DELETE
 */
{
  /* RECV_CH4 */
  high_low[RECV_CH4] = readPulse(RECV_CH4);
  if (high_low[RECV_CH4] == HIGH) {
    return;
  }
  if (1500 < input_pulse_length[RECV_CH4]) {
    /* button push == DELETE */
    if (button_status[BT_DELETE] != DELETE) {
      /* last button state is not DELETE. so, state change to DELETE and delete_counter ++. */
      delete_counter ++;
      button_status[BT_DELETE] = DELETE;
    }
  } else {
    /* button push != DELETE */
    button_status[BT_DELETE] = !DELETE;
  }
}

void onSignalChanged5(void)
{
  /* PCA9685_CH1 */
  high_low[PCA9685_CH1] = readPulse(PCA9685_CH1);
  if (status[ST_MODE] == PCA9685 && status[ST_PING] == ALIVE && status[ST_FORCE_RECEIVER] != FORCE) {
    /* if ST_MODE == PCA9685 and SYSTEM is alive and not FORCE_RECEIVER */
    digitalWriteFast(PWM_OUTPUT_PIN[OUTPUT_CH1], high_low[PCA9685_CH1]);
  }
}

void onSignalChanged6(void)
{
  /* PCA9685_CH2 */
  high_low[PCA9685_CH2] = readPulse(PCA9685_CH2);
  if (status[ST_MODE] == PCA9685 && status[ST_PING] == ALIVE && status[ST_FORCE_RECEIVER] != FORCE) {
    /* if ST_MODE == PCA9685 and SYSTEM is alive and not FORCE_RECEIVER */
    digitalWriteFast(PWM_OUTPUT_PIN[OUTPUT_CH2], high_low[PCA9685_CH2]);
  }
}

void onSignalChanged7(void)
/*
 * ST_PING
 */
{
  /* SYSTEM is alive */
  micros_last[SYSTEM_CH1] = micros();
  status[ST_PING] = ALIVE;
}


void set_led_on(int i, int value, bool force_update)
{
  if (led_configs[i][1] != LED_ON || force_update)
    {
      led_configs[i][1] = LED_ON;
      led_configs[i][4] = value;
      led_configs[i][5] = 0; /* LED CURRENT VALUE */
    }
}

void set_led_off(int i, bool force_update)
{
  if (led_configs[i][1] != LED_OFF || force_update)
    {
      led_configs[i][1] = LED_OFF;
      led_configs[i][4] = LED_POWER_MAX;
      led_configs[i][5] = 0; /* LED CURRENT VALUE */
    }
}

void set_led_fluctuation(int i, int hz, int value, bool force_update)
{
  if (led_configs[i][1] != LED_FLUCTUATION || force_update)
    {
      led_configs[i][1] = LED_FLUCTUATION;
      led_configs[i][2] = hz;
      led_configs[i][4] = value;
      led_configs[i][5] = 0; /* LED CURRENT VALUE */
      led_configs[i][6] = 1;
    }
}

void set_led_blink(int i, int value, bool force_update)
{
  if (led_configs[i][1] != LED_BLINK || force_update)
    {
      led_configs[i][1] = LED_BLINK;
      led_configs[i][2] = LED_BLINK_HZ;
      led_configs[i][3] = LED_POWER_MIN;
      led_configs[i][4] = value;
      led_configs[i][5] = 0; /* LED CURRENT VALUE */
      led_configs[i][8] = 2; /* BLINK_ON_TIMES */
      led_configs[i][9] = 1; /* BLINK_ON_HZ_LENGTH */
      led_configs[i][10] = 9; /* BLINK_OFF_HZ_LENGTH */
      led_configs[i][11] = 0; /* BLINK_CURRENT_HZ */
      led_configs[i][12] = 0; /* BLINK_ON_COUNT */
      led_configs[i][13] = 0; /* BLINK_CURRENT_STATUS */
    }
}

void set_led_blink2(int i, int value, bool force_update)
{
  if (led_configs[i][1] != LED_BLINK2 || force_update)
    {
      led_configs[i][1] = LED_BLINK2;
      led_configs[i][2] = LED_BLINK2_HZ;
      led_configs[i][3] = LED_POWER_MIN;
      led_configs[i][4] = value;
      led_configs[i][5] = 0; /* LED CURRENT VALUE */
      led_configs[i][8] = 2; /* BLINK_ON_TIMES */
      led_configs[i][9] = 4; /* BLINK_ON_HZ_LENGTH */
      led_configs[i][10] = 36; /* BLINK_OFF_HZ_LENGTH */
      led_configs[i][11] = 0; /* BLINK_CURRENT_HZ */
      led_configs[i][12] = 0; /* BLINK_ON_COUNT */
      led_configs[i][13] = 0; /* BLINK_CURRENT_STATUS */
    }
}

void set_led_blink_x_blink(int i, int value, bool force_update)
{
  if (led_configs[i][1] != LED_BLINK_X_BLINK || force_update)
    {
      set_led_blink(i, value, true);
      led_configs[i][1] = LED_BLINK_X_BLINK;
      led_configs[i][2] = LED_BLINK_HZ;
      led_configs[i][3] = LED_POWER_MIN;
      led_configs[i][4] = LED_POWER_MAX;
      led_configs[i][5] = 0; /* LED CURRENT VALUE */
      led_configs[i][14] = 6; /* BB_LENGTH */
      led_configs[i][15] = 4; /* BB_ON_TIMES */
      led_configs[i][16] = 1; /* BB_ON_HZ_LENGTH */
      led_configs[i][17] = 0; /* BB_OFF_HZ_LENGTH */
      led_configs[i][18] = 0; /* BB_GLOBAL_HZ */
      led_configs[i][19] = 0; /* BB_ON_COUNT */
      led_configs[i][20] = 0; /* BB_CURRENT_STATUS */
      led_configs[i][21] = 0; /* BB_LOCAL_HZ */
    }
}

void led_fluctuation(int i)
{
  if (led_configs[i][6] % led_configs[i][7] != 0) {
    if (led_configs[i][6]*led_configs[i][7] < 0) {
      led_configs[i][7] = -1*led_configs[i][6];
    } else {
      led_configs[i][7] = led_configs[i][6];
    }
  }
  next_value = led_configs[i][5] + led_configs[i][7];
  increment = led_configs[i][7];
  if(next_value < led_configs[i][3]) {
    next_value = led_configs[i][3];
    increment *= -1;
  } else if (next_value > led_configs[i][4]) {
    next_value = led_configs[i][4];
    increment *= -1;
  }
  led_configs[i][5] = next_value;
  led_configs[i][7] = increment;
  analogWrite(LED_PIN[i], int(led_configs[i][5]));
}

void led_blink(int i)
{
  /* LED_CONFIG:
   * PIN, PATTERN, HZ, MIN, MAX, CURRENT, INCREMENT, CURRENT_INCREMENT, 
   * 8:BLINK_ON_TIMES, BLINK_ON_HZ_LENGTH, BLINK_OFF_HZ_LENGTH, BLINK_CURRENT_HZ, BLINK_ON_COUNT, BLINK_CURRENT_STATUS,
   * 14:BB_LENGTH, BB_ON_TIMES, BB_ON_HZ_LENGTH, BB_OFF_HZ_LENGTH, BB_GLOBAL_HZ, BB_ON_COUNT, BB_CURRENT_STATUS, BB_LOCAL_HZ
   */
  if ((led_configs[i][8] > led_configs[i][12])
      && (
          (led_configs[i][11] % (led_configs[i][9]+led_configs[i][10]+1)) == 0)) {
    /* LED ON NOW */
    led_configs[i][13]  = 1; /* LED STATUS = ON */
    led_configs[i][12] += 1; /* LED ON COUNT + 1 */
  }
  else if (
           (led_configs[i][11] % (led_configs[i][9]+led_configs[i][10]+1)) == led_configs[i][9]) {
    /* LED OFF NOW */
    led_configs[i][13]  = 0; /* LED STATUS = OFF */
  }

  if (led_configs[i][13]) {
    /* LED ON */
    analogWrite(LED_PIN[i], led_configs[i][4]);
  } else {
    /* LED OFF */
    analogWrite(LED_PIN[i], led_configs[i][3]);
  }

  led_configs[i][11] += 1; /* LED CURRENT HZ +1 */
  if (led_configs[i][11] == LED_BLINK_HZ) {
    led_configs[i][11] = 0;
    led_configs[i][12] = 0;
  }
}

void led_blink_x_blink(int i)
{
  /* LED_CONFIG:
   * PIN, PATTERN, HZ, MIN, MAX, CURRENT, INCREMENT, CURRENT_INCREMENT, 
   * 8:BLINK_ON_TIMES, BLINK_ON_HZ_LENGTH, BLINK_OFF_HZ_LENGTH, BLINK_CURRENT_HZ, BLINK_ON_COUNT, BLINK_CURRENT_STATUS,
   * 14:BB_LENGTH, BB_ON_TIMES, BB_ON_HZ_LENGTH, BB_OFF_HZ_LENGTH, BB_GLOBAL_HZ, BB_ON_COUNT, BB_CURRENT_STATUS, BB_LOCAL_HZ
   */
  if (led_configs[i][18] % LED_BLINK_HZ == 0) {
    if ((led_configs[i][15] > led_configs[i][19])
        && (
            (led_configs[i][21] % (led_configs[i][16]+led_configs[i][17])) == 0)) {
      /* BLINK ON NOW */
      led_configs[i][20]  = 1; /* BLINK STATUS = ON */
      led_configs[i][19] += 1; /* BLINK ON COUNT +1 */
    } else if (
               (led_configs[i][21] % (led_configs[i][16]+led_configs[i][17])) == led_configs[i][16]) {
      /* BLINK OFF NOW */
      led_configs[i][20]  = 0; /* BLINK STATUS = OFF */
    } else {
      /* BLINK OFF NOW */
      led_configs[i][20]  = 0; /* BLINK STATUS = OFF */
    }

    led_configs[i][21] += 1; /* BLINK LOCAL HZ +1 */
  }

  if (led_configs[i][20]) {
    /* BLINK ON */
    led_blink(i);
  } else {
    /* BLINK OFF */
  }

  
  led_configs[i][18] += 1; /* BLINK GLOBAL HZ +1 */
  if (led_configs[i][18] == LED_BLINK_HZ * led_configs[i][14]) {
    led_configs[i][18] = 0;
    led_configs[i][19] = 0;
    led_configs[i][21] = 0;
  }
}


void led_on(int i)
{
  analogWrite(LED_PIN[i], led_configs[i][4]);
}

void led_off(int i)
{
  analogWrite(LED_PIN[i], 0);
}

void led_control(void)
{
  /* LED_CONFIG:
   * PIN, PATTERN, HZ, MIN, MAX, CURRENT, INCREMENT, CURRENT_INCREMENT, 
   * 8:BLINK_ON_TIMES, BLINK_ON_HZ_LENGTH, BLINK_OFF_HZ_LENGTH, BLINK_CURRENT_HZ, BLINK_ON_COUNT, BLINK_CURRENT_STATUS,
   * 14:BB_LENGTH, BB_ON_TIMES, BB_ON_HZ_LENGTH, BB_OFF_HZ_LENGTH, BB_GLOBAL_HZ, BB_ON_COUNT, BB_CURRENT_STATUS, BB_LOCAL_HZ
   */
  for(int i=0; i<NUM_LEDS; i++)
    {
      if (hz_counter %  (LOOP_HZ/led_configs[i][2]) == 0) {
        switch(led_configs[i][1])
          {
          case LED_FLUCTUATION:
            led_fluctuation(i);
            break;
          case LED_BLINK:
            led_blink(i);
            break;
          case LED_BLINK2:
            led_blink(i);
            break;
          case LED_BLINK_X_BLINK:
            led_blink_x_blink(i);
            break;
          case LED_ON:
            led_on(i);
            break;
          case LED_OFF:
            led_off(i);
            break;
          default:
            break;
          }
      }
    }
}

void loop()
{
  if (hz_counter % (LOOP_HZ/PWM_OUT_HZ) == 0)
    {
      /*
       * PWM PROCESSING
       */

      /* if operand order is really important in pararell processing.(interruption)
       * if (A && B) {}: the order is, if A is true then check B.
       * NG pattern:
       * A and B are unsigned long. because micros() returns unsigned long.
       * if (A > B) {
       *   if (A - B > 0) {
       *     here, A must be larger than B. but interruption can occur before this if-operand.
       *     then, A < B. therefore A - B < 0? - NO! these are unsigned long! A - B is OVERFLOW.
       *     A - B is too large value such as 429496728.
       *   }
       * }
       */

#if USE_SYSTEM_PING
      if ((micros_last[CURRENT] - micros_last[SYSTEM_CH1] > 20000) /* SYSTEM no signal more than 20*1000 microseconds. OR minus value because unsigned long. */
          && (micros_last[CURRENT] > micros_last[SYSTEM_CH1])) /* check minus value. if interrupt occured after getting the micros_last[CURRENT] time, then signal is ALIVE. */
        {
          /* system no signal */
          if(status[ST_PING] == ALIVE) {
            status[ST_PING] = DEAD;
          }
        }
      else
        {
          /* system with signal */
          if(status[ST_PING] == DEAD) {
            status[ST_PING] = ALIVE;
          }
        }
#endif
      /* PCA9685 no signal */
      if ((micros_last[CURRENT] - micros_last[PCA9685_CH1] > 20000) /* PCA9685 no signal more than 20*1000 microseconds. OR minus value because unsigned long. */
          && (micros_last[CURRENT] > micros_last[PCA9685_CH1])) /* check minus value. if interrupt occured after getting the micros_last[CURRENT] time, then signal is ALIVE. */
        {
          /* switch to dead */
          if (signal_alive[PCA9685_CH1] == ALIVE) {
            /* PULSE CLEAR */
            signal_alive[PCA9685_CH1] = DEAD;
            input_pulse_length[PCA9685_CH1] = 0;
            input_pulse_length[PCA9685_CH2] = 0;
          }

          /* PCA9685 MODE */
          if (status[ST_MODE] == PCA9685 && status[ST_FORCE_RECEIVER] != FORCE) {
            if (DEBUG) Serial.print("PCA9685 MODE no signal ");

            /* with system signal */
            if (status[ST_PING] == ALIVE) {
              /* LED */
              set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, false);
              set_led_blink_x_blink(HEAD_LIGHT, LED_POWER_MAX, false);
              set_led_fluctuation(BRAKE_LIGHT, 255, LED_POWER_MAX, false);
            }
            /* no system signal */
            else {
              /* LED */
              set_led_blink(HEAD_LIGHT, LED_POWER_MAX, false);
              set_led_blink(BRAKE_LIGHT, LED_POWER_MAX, false);
            }
          } /* else: RECEIVER MODE. nothing to do. */
        }

      /* PCA9685 with signal */
      else
        {
          /* switch to alive */
          if (signal_alive[PCA9685_CH1] == DEAD) {
            signal_alive[PCA9685_CH1] = ALIVE;
          }
          /* PCA9685 MODE */
          if (status[ST_MODE] == PCA9685 && status[ST_FORCE_RECEIVER] != FORCE) {
            if (DEBUG) Serial.print("PCA9685 MODE with signal ");
            /* with system signal */
            if (status[ST_PING] == ALIVE) {
              /* LED */
            if (
                speed_status == SPEED_BRAKE &&
#if !REVERSE
                (speed_up_range[1] <= input_pulse_length[PCA9685_CH2] && input_pulse_length[PCA9685_CH2] <= speed_up_range[0])
#else
                (speed_up_range[0] <= input_pulse_length[PCA9685_CH2] && input_pulse_length[PCA9685_CH2] <= speed_up_range[1])
#endif
                )
              {
                /* NEUTRAL */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_blink_x_blink(HEAD_LIGHT, LED_POWER_MAX, true);
                set_led_fluctuation(BRAKE_LIGHT, LED_POWER_MAX, LED_POWER_MAX, true);
                if (DEBUG) Serial.print("UP TO NEUTRAL");
                speed_status = SPEED_NEUTRAL;
              }
            else if (
                     (speed_status == SPEED_TOP ||
                      speed_status == SPEED_MIDDLE) &&
#if !REVERSE
                     (speed_down_range[1] <= input_pulse_length[PCA9685_CH2] && input_pulse_length[PCA9685_CH2] <= speed_down_range[0])
#else
                     (speed_down_range[0] <= input_pulse_length[PCA9685_CH2] && input_pulse_length[PCA9685_CH2] <= speed_down_range[1])
#endif
                     )
              {
                /* NEUTRAL */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_blink_x_blink(HEAD_LIGHT, LED_POWER_MAX, true);
                set_led_fluctuation(BRAKE_LIGHT, LED_POWER_MAX, LED_POWER_MAX, true);
                if (DEBUG) Serial.print("DOWN TO NEUTRAL");
                speed_status = SPEED_NEUTRAL;
              }
            else if (
                     (speed_status == SPEED_NEUTRAL ||
                      speed_status == SPEED_BRAKE) &&
#if !REVERSE
                     (speed_up_range[2] <= input_pulse_length[PCA9685_CH2] && input_pulse_length[PCA9685_CH2] <= speed_up_range[1])
#else
                     (speed_up_range[1] <= input_pulse_length[PCA9685_CH2] && input_pulse_length[PCA9685_CH2] <= speed_up_range[2])
#endif
                     )
              {
                /* MIDDLE SPEED */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_on(HEAD_LIGHT, LED_POWER_3, true);
                set_led_on(BRAKE_LIGHT, LED_POWER_3, true);
                if (DEBUG) Serial.print("UP TO MIDDLE");
                speed_status = SPEED_MIDDLE;
              }
            else if (
                     speed_status == SPEED_TOP &&
#if !REVERSE
                     (speed_down_range[2] <= input_pulse_length[PCA9685_CH2] && input_pulse_length[PCA9685_CH2] <= speed_down_range[1])
#else
                     (speed_down_range[1] <= input_pulse_length[PCA9685_CH2] && input_pulse_length[PCA9685_CH2] <= speed_down_range[2])
#endif
                     )
              {
                /* MIDDLE SPEED */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_on(HEAD_LIGHT, LED_POWER_3, true);
                set_led_on(BRAKE_LIGHT, LED_POWER_3, true);
                if (DEBUG) Serial.print("DOWN TO MIDDLE");
                speed_status = SPEED_MIDDLE;
              }
            else if (
                     (speed_status == SPEED_MIDDLE ||
                      speed_status == SPEED_NEUTRAL ||
                      speed_status == SPEED_BRAKE) &&
#if !REVERSE
                     input_pulse_length[PCA9685_CH2] <= speed_up_range[2])
#else
                     input_pulse_length[PCA9685_CH2] >= speed_up_range[2])
#endif
              {
                /* TOP SPEED */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_blink2(HEAD_LIGHT, LED_POWER_MAX, true);
                set_led_blink2(BRAKE_LIGHT, LED_POWER_MAX, true);
                if (DEBUG) Serial.print("UP TO TOP");
                speed_status = SPEED_TOP;
              }
            else if (
                     (speed_status == SPEED_NEUTRAL ||
                      speed_status == SPEED_MIDDLE ||
                      speed_status == SPEED_TOP) &&
#if !REVERSE
                     speed_down_range[0] <= input_pulse_length[PCA9685_CH2])
#else
                     speed_down_range[0] >= input_pulse_length[PCA9685_CH2])
#endif
              {
                /* BRAKE */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_on(HEAD_LIGHT, LED_POWER_MAX, true);
                set_led_on(BRAKE_LIGHT, LED_POWER_MAX, true);
                if (DEBUG) Serial.print("DOWN TO BRAKE");
                speed_status = SPEED_BRAKE;
              }
            else
              {
                if (DEBUG) Serial.print("OTHER");
              }
            }
            /* no system signal */
            else {
              /* LED */
              set_led_blink(HEAD_LIGHT, LED_POWER_MAX, false);
              set_led_blink(BRAKE_LIGHT, LED_POWER_MAX, false);
            }
          }
        }


      /* RECEIVER no signal */
      if ((micros_last[CURRENT] - micros_last[RECV_CH1] > 20000) /* RECEIVER no signal more than 20*1000 microseconds. OR minus value because unsigned long. */
          && (micros_last[CURRENT] > micros_last[RECV_CH1])) /* check minus value. if interrupt occured after getting the micros_last[CURRENT] time, then signal is ALIVE. */
        {
          /* switch to dead */
          if (signal_alive[RECV_CH1] == ALIVE) {
            /* PULSE CLEAR */
            signal_alive[RECV_CH1] = DEAD;
            input_pulse_length[RECV_CH1] = 0;
            input_pulse_length[RECV_CH2] = 0;
            input_pulse_length[RECV_CH3] = 0;
            input_pulse_length[RECV_CH4] = 0;
            /* when receiver is no signal, machine must be stopped. */
            status[ST_MODE]             = RECEIVER;
            status[ST_FORCE_RECEIVER]   = !FORCE;
            status[ST_MANUAL_STEERING]  = !FORCE;
            status[ST_MANUAL_THROTTLE]  = !FORCE;
          }

          /* RECEIVER MODE */
          if (status[ST_MODE] == RECEIVER || status[ST_FORCE_RECEIVER] == FORCE) {
            if (DEBUG) Serial.print("RECEIVER MODE no signal ");
            /* LED */
            set_led_blink_x_blink(SYSTEM_LIGHT, LED_POWER_MAX, false);
            set_led_blink_x_blink(HEAD_LIGHT, LED_POWER_MAX, false);
            set_led_fluctuation(BRAKE_LIGHT, LED_POWER_MAX, LED_POWER_MAX, false);
          }
        }

      /* RECEIVER with signal */
      else
        {
          /* switch to alive */
          if (signal_alive[RECV_CH1] == DEAD) {
            signal_alive[RECV_CH1] = ALIVE;
          }
          /* RECEIVER ST_MODE */
          if (status[ST_MODE] == RECEIVER || status[ST_FORCE_RECEIVER] == FORCE) {
            if (DEBUG) Serial.print("RECEIVER MODE with signal ");
            if (
                speed_status == SPEED_BRAKE &&
#if !REVERSE
                (speed_up_range[1] <= input_pulse_length[RECV_CH2] && input_pulse_length[RECV_CH2] <= speed_up_range[0])
#else
                (speed_up_range[0] <= input_pulse_length[RECV_CH2] && input_pulse_length[RECV_CH2] <= speed_up_range[1])
#endif
                )
              {
                /* NEUTRAL */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_blink_x_blink(HEAD_LIGHT, LED_POWER_MAX, true);
                set_led_fluctuation(BRAKE_LIGHT, LED_POWER_MAX, LED_POWER_MAX, true);
                if (DEBUG) Serial.print("UP TO NEUTRAL");
                speed_status = SPEED_NEUTRAL;
              }
            else if (
                     (speed_status == SPEED_TOP ||
                      speed_status == SPEED_MIDDLE) &&
#if !REVERSE
                     (speed_down_range[1] <= input_pulse_length[RECV_CH2] && input_pulse_length[RECV_CH2] <= speed_down_range[0])
#else
                     (speed_down_range[0] <= input_pulse_length[RECV_CH2] && input_pulse_length[RECV_CH2] <= speed_down_range[1])
#endif
                     )
              {
                /* NEUTRAL */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_blink_x_blink(HEAD_LIGHT, LED_POWER_MAX, true);
                set_led_fluctuation(BRAKE_LIGHT, LED_POWER_MAX, LED_POWER_MAX, true);
                if (DEBUG) Serial.print("DOWN TO NEUTRAL");
                speed_status = SPEED_NEUTRAL;
              }
            else if (
                     (speed_status == SPEED_NEUTRAL ||
                      speed_status == SPEED_BRAKE) &&
#if !REVERSE
                     (speed_up_range[2] <= input_pulse_length[RECV_CH2] && input_pulse_length[RECV_CH2] <= speed_up_range[1])
#else
                     (speed_up_range[1] <= input_pulse_length[RECV_CH2] && input_pulse_length[RECV_CH2] <= speed_up_range[2])
#endif
                     )
              {
                /* MIDDLE SPEED */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_on(HEAD_LIGHT, LED_POWER_3, true);
                set_led_on(BRAKE_LIGHT, LED_POWER_3, true);
                if (DEBUG) Serial.print("UP TO MIDDLE");
                speed_status = SPEED_MIDDLE;
              }
            else if (
                     speed_status == SPEED_TOP &&
#if !REVERSE
                     (speed_down_range[2] <= input_pulse_length[RECV_CH2] && input_pulse_length[RECV_CH2] <= speed_down_range[1])
#else
                     (speed_down_range[1] <= input_pulse_length[RECV_CH2] && input_pulse_length[RECV_CH2] <= speed_down_range[2])
#endif
                     )
              {
                /* MIDDLE SPEED */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_on(HEAD_LIGHT, LED_POWER_3, true);
                set_led_on(BRAKE_LIGHT, LED_POWER_3, true);
                if (DEBUG) Serial.print("DOWN TO MIDDLE");
                speed_status = SPEED_MIDDLE;
              }
            else if (
                     (speed_status == SPEED_MIDDLE ||
                      speed_status == SPEED_NEUTRAL ||
                      speed_status == SPEED_BRAKE) &&
#if !REVERSE
                     input_pulse_length[RECV_CH2] <= speed_up_range[2])
#else
                     input_pulse_length[RECV_CH2] >= speed_up_range[2])
#endif
              {
                /* TOP SPEED */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_blink2(HEAD_LIGHT, LED_POWER_MAX, true);
                set_led_blink2(BRAKE_LIGHT, LED_POWER_MAX, true);
                if (DEBUG) Serial.print("UP TO TOP");
                speed_status = SPEED_TOP;
              }
            else if (
                     (speed_status == SPEED_NEUTRAL ||
                      speed_status == SPEED_MIDDLE ||
                      speed_status == SPEED_TOP) &&
#if !REVERSE
                     speed_down_range[0] <= input_pulse_length[RECV_CH2])
#else
                     speed_down_range[0] >= input_pulse_length[RECV_CH2])
#endif
              {
                /* BRAKE */
                set_led_fluctuation(SYSTEM_LIGHT, LED_POWER_4, LED_POWER_4, true);
                set_led_on(HEAD_LIGHT, LED_POWER_MAX, true);
                set_led_on(BRAKE_LIGHT, LED_POWER_MAX, true);
                if (DEBUG) Serial.print("DOWN TO BRAKE");
                speed_status = SPEED_BRAKE;
              }
            else
              {
                if (DEBUG) Serial.print("OTHER");
              }
          }
        }

      if (delete_counter > 0) {
        delete_counter --;
        status[ST_DELETE] = DELETE;
      } else {
        status[ST_DELETE] = !DELETE;
      }

#if DEBUG
      unsigned long analog_ch1 = input_pulse_length[RECV_CH1]*60*4096/1000000;
      unsigned long analog_ch2 = input_pulse_length[RECV_CH2]*60*4096/1000000;
      Serial.print("[");
      Serial.print(status[ST_MODE]);
      Serial.print(",");
      Serial.print(status[ST_DELETE]);
      Serial.print(",");
      Serial.print(status[ST_PING]);
      Serial.print(",");
      Serial.print(status[ST_FORCE_RECEIVER]);
      Serial.print(",");
      Serial.print(status[ST_MANUAL_STEERING]);
      Serial.print(",");
      Serial.print(status[ST_MANUAL_THROTTLE]);
      Serial.print(",");
      Serial.print(input_pulse_length[RECV_CH1]);
      Serial.print(",");
      Serial.print(input_pulse_length[RECV_CH2]);
      Serial.print(",");
      Serial.print(input_pulse_length[RECV_CH3]);
      Serial.print(",");
      Serial.print(input_pulse_length[RECV_CH4]);
      Serial.print(",");
      Serial.print(input_pulse_length[PCA9685_CH1]);
      Serial.print(",");
      Serial.print(input_pulse_length[PCA9685_CH2]);
      Serial.print(",");
      Serial.print(",");
      Serial.print(analog_ch1);
      Serial.print(",");
      Serial.print(analog_ch2);
      Serial.print(",");
      Serial.print(micros_interval);
      Serial.print(",");
      Serial.print(micros_slept);
      Serial.print(",");
      Serial.print(hz_counter);
      Serial.print(",");
      Serial.print(STEERING_PULSE_LENGTH_MIN_THRESHOLD);
      Serial.print(",");
      Serial.print(STEERING_PULSE_LENGTH_MAX_THRESHOLD);
      Serial.print(",");
      Serial.print(THROTTLE_PULSE_LENGTH_MIN_THRESHOLD);
      Serial.print(",");
      Serial.print(THROTTLE_PULSE_LENGTH_MAX_THRESHOLD);
      Serial.print("]");
      Serial.println("");
#endif

#if USE_JOYSTICK
      if (signal_alive[RECV_CH1] == ALIVE) {
        /* Joystick */
        if (status[ST_DELETE] != DELETE)
          {
            /* DELETE BUTTON NOT PUSHED */
            Joystick.button(2, 0);
          }
        else
          {
            /* DELETE BUTTON PUSHED */
            Joystick.button(2, 1);
          }
        if (status[ST_MODE] == RECEIVER)
          {
            /* RECEIVER MODE */
            Joystick.button(1, 0);
          }
        else
          {
            /* PCA9685 MODE */
            Joystick.button(1, 1);
          }

        Joystick.X(map(input_pulse_length[RECV_CH1],  RECV_CH1_PULSE_LENGTH_MIN, RECV_CH1_PULSE_LENGTH_MAX, 0, 1023));
        Joystick.Y(map(input_pulse_length[RECV_CH2],  RECV_CH2_PULSE_LENGTH_MIN, RECV_CH2_PULSE_LENGTH_MAX, 0, 1023));
      }
#endif

      /* DEBUG PRINT
      for(int i=0; i<len(LED_CONFIG); i++) {
        Serial.print(led_configs[SYSTEM_LIGHT][i]);
        Serial.print(",");
      }
      */
    }

  /*
   * LED PROCESSING
   */
  led_control();

  /*
   * sleep interval
   */
  micros_interval = LOOP_INTERVAL - (micros() - micros_last[CURRENT]);
  if (micros_interval > 0) {
    if (micros_interval > LOOP_INTERVAL) {
      /* minus value */
      micros_interval = 0;
    }
  }
  micros_last[WAKEUP_TIME] = micros() + micros_interval + 4;
  micros_last[DELTA_TIME] = micros();
  while (true) {
    delayMicroseconds(micros_interval); // sleep microseconds
    if (micros() >= micros_last[WAKEUP_TIME]) {
      micros_slept = micros() - micros_last[DELTA_TIME];
      break;
    }
    micros_interval = 4;
  }
  micros_last[CURRENT] = micros();

  hz_counter += 1;
  if (hz_counter >= LOOP_HZ) // I found hz_counter starts over 600000000. 
    {
      hz_counter = 0;
    }
}

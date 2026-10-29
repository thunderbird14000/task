// SPDX-License-Identifier: MIT
/* SPDX-FileCopyrightText: 2026 thunderbird1400@gmx.de */
/*
 * tempsens.c - minimal example
 *
 * start    main(): setup via tempsens_setup(), then run tempsens_cycle() every 100 ms
 * core     hardware independent base routines: read_eeprom(), read_temperature(), set_leds()
 * driver   mocks of hw access: driver_read_eeprom(), driver_read_sensor(), driver_set_led()
 *
*/
#include <stdio.h>
#include <time.h>

#define TYPE_A          1           /* Type-A: 1:1 */
#define TYPE_B          2           /* Type-B: 1:10 */

#define GREEN_FROM      5          /* green from 5.0 (below 5.0: red) */
#define YELLOW_FROM     85         /* yellow from 85.0 */
#define RED_FROM        105        /* red from 105.0 */
#define PERIOD_NS       100000000   /* one cycle = 100 ms*/

enum led { GREEN, YELLOW, RED};
static const char * const color[] = { "green", "yellow", "red"};

/* tiny simulation*/
#define CYCLES          5           /* simulate 5 cycles */
#define EEPROM          TYPE_A      /* fixed to Type-A by EEPROM */
/* mock sensor values in °C */
static const long mock_temp[CYCLES] = { 2, 9, 0, 11, 102 };
/* cycle counter for emulation */
static int cycle_no;

/**
 * driver_read_eeprom()
 *
 * EEPROM access simulation: this would be replaced by an i2c call
 *
 * Return: the type of the sensor
 */
static int driver_read_eeprom(void) {
    return EEPROM;
}

/**
 * driver_read_temp_raw()
 *
 * Sensor access simulation: this would be replaced by an i2c call
 *
 * Return: the raw value of the sensor
 */
static long driver_read_temp_raw(void) {
    return mock_temp[cycle_no];
}

/**
 * driver_set_led()
 * @led: input; select one of the LEDs from the enum
 * @on: input; 1 = on, 0 = off
 *
 * LED access simulation: this would be replaced by a sysfs request
 */
static void driver_set_led(int led, int on) {
    if(on)
        printf("    LED %s on\n", color[led]);
}


static int divisor; /* the scale, defined by the sensor type (1, 10) */


/**
 * read_eeprom()
 *
 * extract the sensor type from EEPROM
 *
 * Return: Type-A, Type-B or 0 if not valid
 */
static int read_eeprom(void) {
    int type = driver_read_eeprom();
    if (type == TYPE_A || type == TYPE_B )
        return type;
    return 0;
}

/**
 * read_temp()
 *
 * get the temperature by scaling the sensors raw value by the sensor scale
 *
 * Return: Temperature in °C
 */
static int read_temp(void) {
    long temp_raw = driver_read_temp_raw();
    return temp_raw / divisor;
}

/**
 * set_leds()
 * @on: input, which LED from enum led to switch on
 *
 * switch one led on, the other two off
 *
 */
static void set_leds(int on) {
    for (int i = GREEN; i <= RED; i++)
        driver_set_led(i, i == on);
}

/**
 * tempsens_setup()
 *
 * setup: read the EEPROM, get the type and configure measurements
 *
 */
static void tempsens_setup(void) {
    switch (read_eeprom()) {
        case TYPE_A:
            divisor = 1;
            break;
        case TYPE_B:
            divisor = 10;
            break;
        default: /*error*/
            divisor = 0;
            set_leds(RED);
            break;
    }
}

/**
 * tempsens_cycle()
 *
 * main "thread"; every 100ms read the sensor, switch corresponding LEDs
 */
static void tempsens_cycle(void) {
    int temp = read_temp();
    if (temp < GREEN_FROM)
        set_leds(RED);
    else if (temp < YELLOW_FROM)
        set_leds(GREEN);
    else if (temp < RED_FROM)
        set_leds(YELLOW);
    else
        set_leds(RED);
}

/**
 * main()
 *
 * setup, then run the simulation
 */
int main(void) {
    struct timespec period;
    period.tv_sec = 0;
    period.tv_nsec = PERIOD_NS;

    tempsens_setup();
    for(cycle_no = 0; cycle_no < CYCLES; cycle_no++) {
        printf("cycle %d\n", cycle_no + 1);
        tempsens_cycle();
        nanosleep(&period, NULL);
    }
    return 0;
}
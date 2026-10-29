// SPDX-License-Identifier: MIT
/* SPDX-FileCopyrightText: 2026 thunderbird1400@gmx.de */
/*
 * tasks-stub.c - minimal example on kernel drivers and stubs
 *
 * start    main(): setup via tempsens_setup(), then run tempsens_cycle() every 100 ms
 * core     hardware independent base routines: read_eeprom(), read_temperature(), set_leds()
 * driver   sysfs files of the kernel drivers: driver_read_eeprom() (at24),
 *          driver_read_temp_raw() (lm75), driver_set_led() (LED class)
 *
 * Same as task.c, only the three driver functions differ. The
 * devices below the drivers are stubs: at24 and lm75 on i2c-stub, the LEDs
 * from uleds; task-stub-test sets them up and links the driver
 * files to the fixed paths below (the bus and hwmon numbers change).
 */
#include <pthread.h>
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

#define CYCLES          5           /* run 5 cycles */
/* cycle counter */
static int cycle_no;

/* the driver files, linked here by task-stub-test */
#define EEPROM_FILE     "/tmp/task-stub/eeprom"        /* at24 */
#define TEMP_FILE       "/tmp/task-stub/temp1_input"   /* lm75, millidegrees */
#define LED_FILE        "/sys/class/leds/task-sim-%s/brightness"  /* uleds */
#define TYPE_OFFSET     5           /* the type byte in the record "TSEN" 1 type ... */
#define SENSOR_ERROR    (-1000)     /* no answer: far below any threshold, so red */

/**
 * driver_read_eeprom()
 *
 * EEPROM access: the type byte of the record through the at24 driver
 *
 * Return: the type of the sensor, 0 if the EEPROM cannot be read
 */
static int driver_read_eeprom(void) {
    FILE *file = fopen(EEPROM_FILE, "r");
    int type = 0;

    if (!file)
        return 0;
    if (fseek(file, TYPE_OFFSET, SEEK_SET) == 0) {
        type = fgetc(file);
        if (type == EOF)
            type = 0;
    }
    fclose(file);
    return type;
}

/**
 * driver_read_temp_raw()
 *
 * Sensor access: temp1_input of the lm75 driver (millidegrees)
 *
 * Return: the raw value of the sensor in whole degrees (Type-B: ten times
 * the temperature), SENSOR_ERROR if the sensor does not answer
 */
static long driver_read_temp_raw(void) {
    FILE *file = fopen(TEMP_FILE, "r");
    long mdeg;
    int count;

    if (!file)
        return SENSOR_ERROR;
    count = fscanf(file, "%ld", &mdeg);
    fclose(file);
    if (count != 1)
        return SENSOR_ERROR;
    return mdeg / 1000;
}

/**
 * driver_set_led()
 * @led: input; select one of the LEDs from the enum
 * @on: input; 1 = on, 0 = off
 *
 * LED access: brightness of the LED class device (uleds)
 */
static void driver_set_led(int led, int on) {
    char path[80];
    FILE *file;

    snprintf(path, sizeof(path), LED_FILE, color[led]);
    file = fopen(path, "w");
    if (!file)
        return;
    fprintf(file, "%d\n", on);
    fclose(file);
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
 * tempsens_thread()
 *
 * main "thread"; every 100ms read the sensor, switch corresponding LEDs
 */
static void *tempsens_thread(void *arg) {

    /* only for simulation */
    struct timespec period;
    period.tv_sec = 0;
    period.tv_nsec = PERIOD_NS;

    (void)arg; /* warning */

    /* if something goes wrong during setup, dont try division by 0*/
    if(divisor == 0)
        return NULL;

    for(cycle_no = 0; cycle_no < CYCLES; cycle_no++) {
        printf("cycle %d\n", cycle_no + 1);
        int temp = read_temp();
        printf("cycle %d\n", temp);
        if (temp < GREEN_FROM)
            set_leds(RED);
        else if (temp < YELLOW_FROM)
            set_leds(GREEN);
        else if (temp < RED_FROM)
            set_leds(YELLOW);
        else
            set_leds(RED);
        nanosleep(&period, NULL);
    }
    return NULL;
}

/**
 * main()
 *
 * setup, then run the simulation
 */
int main(void) {
    pthread_t thread;
    //struct timespec run_time;
    //run_time.tv_sec = 0;
    //run_time.tv_nsec = CYCLES * PERIOD_NS - PERIOD_NS / 2; /*kill after 450ms*/

    tempsens_setup();

    pthread_create(&thread, NULL, tempsens_thread, NULL);
    //nanosleep(&run_time, NULL);
    pthread_join(thread, NULL);


    return 0;
}

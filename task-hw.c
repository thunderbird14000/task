// SPDX-License-Identifier: MIT
/* SPDX-FileCopyrightText: 2026 thunderbird1400@gmx.de */
/*
 * task-hw.c - minimal example on the SK-AM64's hardware, from Linux
 *
 * start    main(): setup via tempsens_setup(), then run tempsens_cycle() every 100 ms
 * core     hardware independent base routines: read_eeprom(), read_temperature(), set_leds()
 * driver   raw hardware access: driver_read_eeprom() and driver_read_temp_raw()
 *          over /dev/i2c-0 (EEPROM 0x51, sensor 0x48), driver_set_led() over
 *          the GPIO character device (GPIO0 lines 38, 35, 32)
 *
 * Same as task.c, only the three driver functions differ (and the
 * simulation's cycle limit: here it runs until Ctrl-C). Needs the devicetree
 * k3-am642-sk-tempsens-linux.dtb (Linux owns I2C0 and the three lines) and
 * an LM75-format sensor at 0x48 (TMP100, or the Nucleo in LM75 mode).
 */
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>              /* open */
#include <string.h>             /* memset */
#include <unistd.h>             /* close */
#include <sys/ioctl.h>          /* ioctl: I2C transfers, GPIO requests */
#include <linux/gpio.h>         /* GPIO chardev v2 */
#include <linux/i2c.h>          /* struct i2c_msg */
#include <linux/i2c-dev.h>      /* I2C_RDWR */

#define TYPE_A          1           /* Type-A: 1:1 */
#define TYPE_B          2           /* Type-B: 1:10 */

#define GREEN_FROM      5          /* green from 5.0 (below 5.0: red) */
#define YELLOW_FROM     85         /* yellow from 85.0 */
#define RED_FROM        105        /* red from 105.0 */
#define PERIOD_NS       100000000   /* one cycle = 100 ms*/

enum led { GREEN, YELLOW, RED};
static const char * const color[] = { "green", "yellow", "red"};

#define CYCLES          2147483647  /* hardware: run practically for ever */
/* cycle counter */
static int cycle_no;

#define I2C_FILE        "/dev/i2c-0"        /* I2C0: EEPROM and sensor */
#define GPIO_FILE       "/dev/gpiochip1"    /* GPIO0 (600000.gpio) */
#define EEPROM_ADDR     0x51                /* board EEPROM, 16-bit offsets */
#define TYPE_OFFSET     0xFF05              /* the type byte of the record at 0xFF00 */
#define SENSOR_ADDR     0x48                /* LM75-format sensor, register 0 */
#define SENSOR_ERROR    (-1000)             /* no answer: far below any threshold, so red */

/* LED lines by enum led: green J4 pin 16, yellow pin 7, red pin 15 */
static const unsigned int led_line[] = { 38, 35, 32 };

/* on I2C0: write wlen bytes to addr, then (repeated start) read rlen bytes; 0, -1 */
static int i2c_write_read(int addr, void *wbuf, int wlen, void *rbuf, int rlen) {
    struct i2c_msg msg[2] = {
        { .addr = addr, .flags = 0, .len = wlen, .buf = wbuf },
        { .addr = addr, .flags = I2C_M_RD, .len = rlen, .buf = rbuf },
    };
    struct i2c_rdwr_ioctl_data xfer = { .msgs = msg, .nmsgs = 2 };
    int fd = open(I2C_FILE, O_RDWR);
    int ret;

    if (fd < 0)
        return -1;
    ret = ioctl(fd, I2C_RDWR, &xfer) < 0 ? -1 : 0;
    close(fd);
    return ret;
}

/**
 * driver_read_eeprom()
 *
 * EEPROM access: the type byte of the record, over I2C0
 *
 * Return: the type of the sensor, 0 if the EEPROM does not answer
 */
static int driver_read_eeprom(void) {
    unsigned char offset[2] = { TYPE_OFFSET >> 8, TYPE_OFFSET & 0xFF };
    unsigned char type;

    if (i2c_write_read(EEPROM_ADDR, offset, 2, &type, 1) != 0)
        return 0;
    return type;
}

/**
 * driver_read_temp_raw()
 *
 * Sensor access: the LM75 register (1/256 degC), over I2C0
 *
 * Return: the raw value of the sensor in whole degrees (Type-B: ten times
 * the temperature), SENSOR_ERROR if the sensor does not answer
 */
static long driver_read_temp_raw(void) {
    unsigned char reg = 0;
    unsigned char data[2];
    long raw;

    if (i2c_write_read(SENSOR_ADDR, &reg, 1, data, 2) != 0)
        return SENSOR_ERROR;
    raw = (data[0] << 8) | data[1];
    if (raw >= 0x8000)
        raw -= 0x10000;
    return raw / 256;
}

/**
 * driver_set_led()
 * @led: input; select one of the LEDs from the enum
 * @on: input; 1 = on, 0 = off
 *
 * LED access: the three GPIO lines, requested as outputs at the first call;
 * the LED that goes on is also logged, as in the mock
 */
static void driver_set_led(int led, int on) {
    static int led_fd = -1;
    struct gpio_v2_line_values values;

    if (led_fd < 0) {
        struct gpio_v2_line_request req;
        int chip = open(GPIO_FILE, O_RDWR);

        if (chip < 0)
            return;
        memset(&req, 0, sizeof(req));
        for (int i = GREEN; i <= RED; i++)
            req.offsets[i] = led_line[i];
        req.num_lines = 3;
        req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
        snprintf(req.consumer, sizeof(req.consumer), "tempsens");
        if (ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &req) == 0)
            led_fd = req.fd;
        close(chip);
        if (led_fd < 0)
            return;
    }
    values.mask = 1UL << led;
    values.bits = (unsigned long)on << led;
    ioctl(led_fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
    if (on)
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

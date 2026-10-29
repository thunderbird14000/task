#!/bin/sh
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 thunderbird1400@gmx.de
#
# This script has been created with assistance of AI
#
# task-stub.sh - task-stub against the real kernel drivers
# on stubs: at24 (24c02, record at offset 0) and lm75 (tmp100) on i2c-stub,
# the LEDs task-sim-green/-yellow/-red from uleds. The driver files are
# linked to /tmp/task-stub/ (fixed paths in the program). Each case:
# set EEPROM and sensor, run the program (5 cycles, 0.5 s), check the LEDs.
#
#   sudo ./task-stub-test    (the program must lie next to the script)

TS=$(dirname "$0")/task-stub   # the program: always next to this script
[ -x "$TS" ] || { echo "$TS: not found (it belongs next to this script)"; exit 2; }
DIR=/tmp/task-stub             # the fixed paths of the program
REC=$DIR/uled.bin                       # scratch: struct uleds_user_dev of one LED
LEDS=/sys/class/leds/task-sim       # the LEDs, without -green etc.
BUS=                                    # number of the stub's I2C adapter
PASS=0
FAIL=0

# remove everything: devices, LEDs (closing /dev/uleds), modules, links
cleanup() {
    for a in 0x48 0x51; do
        [ -n "$BUS" ] && [ -e /sys/bus/i2c/devices/$BUS-00${a#0x} ] &&
            echo $a > /sys/bus/i2c/devices/i2c-$BUS/delete_device
    done
    exec 3>&- 4>&- 5>&-
    rmmod uleds i2c-stub 2>/dev/null
    rm -rf $DIR
}

# LED class device $1 on the /dev/uleds open as fd $2: one write of
# struct uleds_user_dev { char name[64]; int max_brightness; } (68 bytes)
make_led() {
    printf "%s" "$1" > $REC
    dd if=/dev/zero bs=$((64 - ${#1})) count=1 2>/dev/null >> $REC
    printf '\001\000\000\000' >> $REC
    dd if=$REC bs=68 count=1 2>/dev/null >&$2
}

# register $1 of the stubbed sensor := $2 (SMBus words LSB first, LM75 MSB first)
set_word() {
    v=$(( $2 & 0xFFFF ))
    i2cset -f -y $BUS 0x48 $1 $(printf 0x%04x $(( ((v & 0xFF) << 8) | (v >> 8) ))) w
}

# sensor := $1 whole degrees (LM75 format: 1/256 degC)
set_temp() {
    set_word 0x00 $(( $1 * 256 ))
}

# the bytes $* into the stub's EEPROM from offset 0, through the at24 file
ee_write() {
    f=
    for b in "$@"; do f="$f$(printf '\\%03o' $b)"; done
    printf "$f" > $DIR/eeprom
}

# record "TSEN" 1, type $1, sensor 0x48, register 0, checksum
write_record() {
    ee_write 0x54 0x53 0x45 0x4E 1 $1 0x48 0 $(( 0x54 ^ 0x53 ^ 0x45 ^ 0x4E ^ 1 ^ $1 ^ 0x48 ))
}

# the LED that is on: g, y, r, - (none) or ? (more than one)
led() {
    case "$(cat $LEDS-green/brightness)$(cat $LEDS-yellow/brightness)$(cat $LEDS-red/brightness)" in
        100) echo g ;; 010) echo y ;; 001) echo r ;; 000) echo - ;; *) echo "?" ;;
    esac
}

# one case: LEDs off, run the program, expect LED $1; $2 describes the case
check() {
    for c in green yellow red; do echo 0 > $LEDS-$c/brightness; done
    "$TS" > $DIR/out.txt 2>&1
    rc=$?
    l=$(led)
    note=
    [ $rc -ne 0 ] && note=" (program ended with $rc: $(tail -n 1 $DIR/out.txt))"
    if [ "$l" = "$1" ]; then
        echo "  ok    $2 -> $l$note"; PASS=$((PASS + 1))
    else
        echo "  FAIL  $2: LED $l, expected $1$note"; FAIL=$((FAIL + 1))
    fi
}

trap cleanup EXIT INT TERM
modprobe i2c-stub chip_addr=0x48,0x51 || { echo "no i2c-stub module"; exit 1; }
modprobe uleds || { echo "no uleds module"; exit 1; }
for m in lm75 at24; do
    modprobe $m 2>/dev/null
    [ -d /sys/bus/i2c/drivers/$m ] || { echo "no $m driver"; exit 1; }
done
mkdir -p $DIR
exec 3<> /dev/uleds 4<> /dev/uleds 5<> /dev/uleds
make_led task-sim-green 3
make_led task-sim-yellow 4
make_led task-sim-red 5
[ -e $LEDS-red ] || { echo "uleds made no LEDs"; exit 1; }
for a in /sys/bus/i2c/devices/i2c-*; do
    grep -q "SMBus stub" $a/name 2>/dev/null && BUS=${a##*/i2c-}
done
[ -n "$BUS" ] || { echo "i2c-stub adapter not found"; exit 1; }
set_temp 20
set_word 0x02 0x4B00                    # power-on limits of a TMP100
set_word 0x03 0x5000
echo tmp100 0x48 > /sys/bus/i2c/devices/i2c-$BUS/new_device
echo 24c02 0x51 > /sys/bus/i2c/devices/i2c-$BUS/new_device
sleep 0.2
# the fixed paths of the program
ln -sf /sys/bus/i2c/devices/$BUS-0051/eeprom $DIR/eeprom
ln -sf $(ls /sys/bus/i2c/devices/$BUS-0048/hwmon/hwmon*/temp1_input) $DIR/temp1_input
[ -e $DIR/temp1_input ] || { echo "lm75 did not bind to the stub"; exit 1; }
echo "i2c-stub on i2c-$BUS: at24 0x51, lm75 0x48; LEDs from uleds"

echo "records"
ee_write 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF 0xFF
check r "erased EEPROM"
write_record 3
check r "type byte 3"

echo "Type-A (1:1)"
write_record 1
for t in 4:r 5:g 20:g 84:g 85:y 104:y 105:r -10:r; do
    set_temp ${t%:*}
    check ${t#*:} "${t%:*} degC"
done

echo "Type-B (1:10): the sensor shows ten times the value (LM75: up to 12 degC)"
write_record 2
for t in 0:r 4:r 5:g 9:g 12:g; do
    set_temp $(( ${t%:*} * 10 ))
    check ${t#*:} "${t%:*} degC (sensor $(( ${t%:*} * 10 )))"
done

echo "sensor removed"
echo 0x48 > /sys/bus/i2c/devices/i2c-$BUS/delete_device
check r "no sensor"

echo "passed $PASS, failed $FAIL"
[ $FAIL -eq 0 ]
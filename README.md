# Thermal Protection Module

Task:
1. I2C based temperature sensor provides raw values
2. Temperature acquisition shall run in a dedicated thread every 100 ms
3. 3 LEDs for temperature indication
4. Direct visual feedback
5. Two sensors
6. EEPROM record to decide on sensor

Implementation:
1. either simple mock (task-mock) or real driver stub [i2cstub + lm75] (task-stub)
2. ok
3. either simple mock (task-mock) or real driver stub [uleds] (task-stub)
4. ok
5. ok
6. either simple mock (task-mock) or real driver stub [i2cstub + at24] (task-stub)

If an TI SK-AM64 Board is available the task-hw.c binary is designed to run
exactly the same as the task-stub.c, just on real hardware.

## Documentation 


The core architecture for all versions is essentially the same (see architecture.png).
While the additional function call does not make sense in case of the simple mock,
it allows for identical core routines between the versions. 

Disclaimer: Due to the complexity of setting up the stubs in a reproducable manner,
parts of the shell script to start the stubs were created with assistance of AI.

A simple 5 step simulation has been built into the code to showcase the task.

## Build and Run

To build, simply call make all.

The stub versions type-a and type-b can be executed by running the binary from
the shell. 
The mock version needs stubs for i2c (at24 eeprom and lm75 sensor) and the LEDs
(uleds). The task-stub.sh script will handle the setup. 

WARNING: While the script has been tested on Ubuntu 24.04, Fedora 44 and
Yocto/Poky 6, it will need root permissions to set up the stubs. Please be
aware that if you care for your machine, you should never execute any unknown
script as root. A demonstration will be given - please be careful IF you decide
to run this test script.

The hw version requires a TI SK-AM64 device, on which all versions were
initially tested and will be demoed on. The software should be able to run on
any Linux device providing an at24 eeprom, a lm75 compatible sensor and three
gpio lines for LEDs. A device tree fragment for the sk am64 board is provided
in this repository.

# Next steps

While 100 ms cycle time is fine for a Linux user space implementation, I propose
an additional solution. As any modern SoC provides embedded realtime cores,
in case of the AM64xx up to four Cortex-R5F cores and a safety-certified M4F
(Most other manufacturers provide similar solutions, Xilinx Ultrascale: R5F, 
NXP i.mx 8/9: depends on the version, ST32MP1: STM32 Compatible Cortex-Mx),
timing critical tasks with low-level hardware access are a no-brainer for
outsourcing. 
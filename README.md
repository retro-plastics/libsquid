# squid

Download pico SDK from

https://github.com/raspberrypi/pico-sdk

use the --recurse option.

Compile with

mkdir build
cd build
cmake -DPICO_BOARD=pico_w ..

Make sure you have lib in /usr/lib/arm-none-eabi. if not, create it with ln -s newlib lib

Don't forget to link gdb-multiarch to arm-none-eabi-gdb
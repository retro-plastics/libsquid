# squid

Download pico SDK from

https://github.com/raspberrypi/pico-sdk

use the --recurse option.

Compile with

mkdir build
cd build
cmake .. -DPICO_BOARD=pico_w

Make sure you have lib in /usr/lib/arm-none-eabi. if not, create it with ln -s newlib lib

Don't forget to link gdb-multiarch to arm-none-eabi-gdb

https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html

For network you need to pass...
-DPICO_BOARD=pico_w -DWIFI_SSID="Your Network" -DWIFI_PASSWORD="Your Password"
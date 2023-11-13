# squid

## Prepare the ARM environment

### Install the compilation tools for arm

~~~bash
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib gdb-multiarch
~~~

### Apply fixes

All the tools will start with `arm-none-eabi` except `gdb-multiarch`, so make it consistent with the rest of the gang.

~~~bash
sudo ln -s /usr/bin/gdb-multiarch /usr/bin/arm-none-eabi-gdb
~~~

Installing the `newlib` might not create the library directory, expected by some older configurations. If the `/usr/lib/arm-none-eabi/lib` is missing, create a link to the newlib like this.

~~~bash
sudo ln -s /usr/lib/arm-none-eabi/newlib /usr/lib/arm-none-eabi/lib
~~~

### More about installing the ARM compilation tools

Read more about ARM compilation tools [here](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html).

## Build OpenOCD

### Install compilation tools 

~~~bash
sudo apt install automake autoconf build-essential texinfo libtool libftdi-dev libusb-1.0-0-dev
~~~

### Compile openOCD

~~~bash
git clone https://github.com/raspberrypi/openocd.git --recurse
./bootstrap
./configure --enable-picoprobe
make -j4
sudo make install
~~~

### More about building the OpenOCD

Read more about building the OpenOCD [here](https://www.digikey.be/en/maker/projects/raspberry-pi-pico-and-rp2040-cc-part-2-debugging-with-vs-code/470abc7efb07432b82c95f6f67f184c0).

## Build the picoprobe

Download the picoprobe.

~~~bash
git clone https://github.com/raspberrypi/picoprobe --recurse
~~~

### Update CMakeLists.txt

Add two lines to the top of CMakeLists.txt, right after the `cmake_minimum_required(...)` statement to automatically fetch the Pico SDK from github, and build for the Pico W.

~~~cmake
cmake_minimum_required(VERSION 3.12)

set(PICO_SDK_FETCH_FROM_GIT on)
set(PICO_BOARD pico_w)
~~~

### Compile for Raspberry PI Pico W.

~~~bash
cd picoprobe
mkdir build
cd build
cmake ..
make
~~~

### Copy picoprobe.of2 to your debugger RPI Pico

It is located in the `picoprobe/build` directory.


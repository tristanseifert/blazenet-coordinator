# BlazeNet RF Firmware
Here lives the firmware that runs on the [EFR32FG23](https://www.silabs.com/wireless/proprietary/efr32fg23-sub-ghz-wireless-soc) RFSoC on the coordinator board. It's responsible for providing the air interface to the host BlazeNet stack, and uses the built in modems in the SoC to interface to the proprietary 868/915MHz air protocol.

## Supported Devices
The following device SKUs are supported:

- EFR32FG23B020F512IM48: 512K flash, +20dBm transmit power

Other chips (for example, the B014 models without the bonus power amplifier) may work, but have not been tested.

## Building
To get started with the code, ensure you have a CMake toolchain file for your ARM toolchain. Inside a build directory (in this case, build) issue the following commands:

```
mkdir build
cmake -G Ninja -B build -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

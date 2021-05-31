# Bleam-Scanner-3

<img align="left" src="https://user-images.githubusercontent.com/44293126/112982869-ab80c700-9165-11eb-9872-a304371a0b83.png" hspace="15" style="float: left">Bleam Scanner is an inverted Bluetooth low energy (BLE) beacon communication protocol. In the inverted communication scheme beacon serves as a scanner, taking this role from a phone that in its turn is responsible for advertising. The main benefit of such a design approach is its ability to function extensively in the background, constantly monitoring RSSI levels.

Bleam Scanner 3 is an upgraded version of [Bleam Scanner 2](https://github.com/Connax-Oy/Bleam-Scanner-2).

## Design drivers:
* Long-term background RSSI measuring, for precise location or proximity solutions; 
* Easy, fast and secure deployment via [Bleam Tools](https://play.google.com/store/apps/details?id=io.connax.bleneurowrite) and [nRF Connect](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&hl=en&gl=US) mobile apps; 
* By default works with [Ble.am](https://ble.am/) mobile SDKs for iOS and Android; but open for 3rd party implementations; 
* Beacon battery lifetime optimisation.

## Getting started

### IDE

It is recommended to use [Segger Embedded Studio](https://www.segger.com/downloads/embedded-studio) for this project.
SES license is free for non-commercial use and also for commercial use with Nordic Semiconductor Cortex-M based devices --
see [SES licensing conditions here](https://www.segger.com/products/development-tools/embedded-studio/license/licensing-conditions).

Please use SES **V4.52c** as it is the latest SES version compatible with nRF SDK 15.3.0.

### Clone repo and submodules

```
git clone https://github.com/Connax-Oy/Bleam-Scanner-3.git
```

Ruuvi SDK is included via git submodules. To search and install submodules from this repository, run

```
git submodule update --init --recursive
```

### nRF SDK and SoftDevice

[Download](https://www.nordicsemi.com/Software-and-tools/Software/nRF5-SDK/Download) both nRF SDK 15.3.0 and nRF SDK 12.3.0.
For each download you will need to pick SDK version (12.3.0, 15.3.0),
uncheck the SoftDevices as they come with the SDK anyway, then click the **Download files (.zip)** button.

Extract SDKs to the Bleam Scanner 3 git repo root; extracted SDK directory names `nRF5_SDK_15.3.0_59ac345` and `nRF5_SDK_12.3.0_d7731ad` are included in gitignore.

### Bootloader

By default, only Bleam Scanner for [RuuviTag](https://ruuvi.com/ruuvitag/) and [IBKS Plus](https://accent-systems.com/product/ibks-plus) support DFU update.
Use [nRF SDK 15.3.0 BLE Secure DFU Bootloader example](https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.sdk5.v15.3.0/ble_sdk_app_dfu_bootloader.html)
and [nRF SDK 12.3.0 BLE Secure DFU Bootloader example](https://infocenter.nordicsemi.com/topic/com.nordic.infocenter.sdk5.v12.3.0/ble_sdk_app_dfu_bootloader.html)
(included with SDK) as templates for your bootloader.

Follow [this tutorial](https://ruuvi.com/ruuvi-firmware-part-12-bootloader/) on how to set up a bootloader for RuuviTag.

### micro-ecc

Bleam Scanner 3 uses [micro-ecc](https://github.com/kmackay/micro-ecc) library for cryptographic operations even when not using the bootloader.
This library binary is not provided with nRF SDK, so you have to build it yourself.

Make sure you have [GNU Arm Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm) installed.

#### Building micro-ecc for nRF52 projects
```
cd nRF5_SDK_15.3.0_59ac345/external/micro-ecc
git clone https://github.com/kmackay/micro-ecc.git
cd nrf52hf_armgcc/armgcc
make
```

#### Building micro-ecc for nRF51 projects
```
cd nRF5_SDK_12.3.0_d7731ad/external/micro-ecc
git clone https://github.com/kmackay/micro-ecc.git
cd nrf51_armgcc/armgcc
make
```

This is the directory selective treeview when the setup is done:
```
\.
|   .gitignore
|   .gitmodules
|   bleam_scanner_3.emProject
|   flash_placement.xml
|   flash_placement_51.xml
|   LICENSE
|   README.md
|   
+---docs
+---include
+---nRF5_SDK_12.3.0_d7731ad
+---nRF5_SDK_15.3.0_59ac345
|                       
+---Ruuvi_SDK
|   +---ruuvi.boards.c
|   +---ruuvi.drivers.c         
|   +---ruuvi.endpoints.c
|   \---ruuvi.libraries.c
|               
\---src
```

## Deployment

Bleam Scanner 3 is developed for five boards:
* [nRF52840](https://infocenter.nordicsemi.com/topic/struct_nrf52/struct/nrf52840.html);
* [nRF52832](https://infocenter.nordicsemi.com/topic/struct_nrf52/struct/nrf52832.html);
* [RuuviTag](https://ruuvi.com/ruuvitag/) based on nRF52832 chipset;
* [nRF51822](https://infocenter.nordicsemi.com/topic/struct_nrf51/struct/nrf51822.html);
* [IBKS Plus](https://accent-systems.com/product/ibks-plus) based on nRF51822 chipset.

### Building

* Open the Bleam Scanner 3 solution `bleam_scanner_3.emProject` in SES;
* Select the project you want to build;
* Select **Release** or **Release With Debug Information** build configuration;
* Build the project.

The build result is located in the `build/` directory under corresponding project name and build configuration.

The bootloader project builds in exactly the same way.

### Flashing

Flash the built `.hex` binaries onto the board via [nrfjprog command line tool](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fug_nrf_cltools%2FUG%2Fcltools%2Fnrf_nrfjprogexe.html)
or [nRF Connect Programmer](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fug_nc_programmer%2FUG%2Fnrf_connect_programmer%2Fncp_introduction.html) GUI application.

#### Flashing application

Just an application requires only application `.hex` binary and a [SoftDevice](https://infocenter.nordicsemi.com/topic/struct_nrf52/struct/nrf52_softdevices.html) `.hex` binary to run.
You can find the SoftDevices in nRF SDK under `SDK-directory/components/softdevice`.

Bleam Scanner firmware for nRF52832 chipset uses `S132` SoftDevice,
while Bleam Scanner firmware for nRF52840 chipset uses `S140` SoftDevice,
and Bleam Scanner firmware for nRF51822 chipset uses `S130` SoftDevice.

SES can flash an application from the project onto the board, and it automatically adds SoftDevice provided with the project settings.

#### Flashing application and a bootloader

Application with a bootloader requires application and bootloader `.hex` binaries, a SoftDevice and **bootloader settings** binary.

Provided in this section are instructions for nRF52 boards.
Flashing a nRF51 board is basically the same, but the family setting will be `NRF51`.

Use [nrfutil command line tool](https://infocenter.nordicsemi.com/index.jsp?topic=%2Fug_nrfutil%2FUG%2Fnrfutil%2Fnrfutil_intro.html) to generate bootloader settings:
```
nrfutil settings generate --family NRF52 --application path-to-app-binary/app-binary.hex --bl-settings-version 1 bl_settings.hex
```

You can use [mergehex command line tool](https://infocenter.nordicsemi.com/topic/ug_nrf_cltools/UG/cltools/nrf_mergehex.html)
to merge all the binaries you need to flash into a single hex file for convenience:
```
mergehex -m file1.hex file2.hex … -o output.hex
```

It is recommended to erase the board before starting flashing.

General command to program with nrfjprog is:
```
nrfjprog -f NRF52 --program output.hex –-chiperase
```

If several boards are connected to your desktop, specify the board with its serial number.

Refer to [nrfjprog command line tool reference](https://infocenter.nordicsemi.com/topic/ug_nrf_cltools/UG/cltools/nrf_nrfjprogexe_reference.html)
or [nRF Connect Programmer instructions](https://infocenter.nordicsemi.com/topic/ug_nc_programmer/UG/nrf_connect_programmer/ncp_programming_dongle.html)
for details on flashing your board.

## Licensing

Bleam Scanner 3 is licenced under MIT License.

## Documentation

Project documentation is generated with Doxygen.
Access the documentation locally from `doc/index.html`.
Doxyfile is included in the repo.

[The Bleam Scanner 2 repository wiki](https://github.com/Connax-Oy/Bleam-Scanner-3/wiki) describes the logic and decisions behind Bleam Scanner 2 solutions,
while [this document](https://github.com/Connax-Oy/Bleam-Scanner-3/blob/main/docs/bleam-scanner-2-3-differences.md) describes
differences and decisions taken between Bleam Scanner 2 and 3.

## How to contribute

Even though the Bleam Scanner was initially created to advance the communication with BLEAM mobile SDK,
it’s open for 3rd party implementations and modifications for own needs.
Bleam Scanner has many potential applications in payments, location, identity, gaming and many others,
so feel free to adopt it to your own needs.

We have a growing development community that’s active on:

* [Slack](https://join.slack.com/t/bleamspace/shared_invite/zt-o1w10ohw-iyzmqOkV24zh_yiYIkEbTw) 
* [Facebook](http://facebook.com/groups/connax/)
* [Twitter](https://twitter.com/bleam_official)
* [Our website](https://ble.am/opensource)

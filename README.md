## About
This repository holds the PCBA files, firmware, and design files for the OpenNerve charger.
The charger is designed to be compatible with the [OpenNerve Implantable Pulse Generator](https://github.com/CARSSCenter/OpenNerve-Implantable-Pulse-Generator) and to inductively charge it at 300kHz.

## Insttructions for Use
BTNs behavior 
Regarding the button functionalities:

    BTN1 changes the state from WAIT to SCAN. If pressed during CHARGE state, it interrupts charging and returns to WAIT.
    BTN2 does not trigger any action in the current firmware.
    BTN3 enables DFU mode and resets the device to boot in DFU mode.


LEDs behavior
The left LED shows the current state:

    OFF → WAIT
    Blue → SCAN
    White → SLOW_CHARGE_AND_SCAN 
    Yellow → CHARGE (connected and charging)

The middle LED turns Green when charging is complete.

Below is a more detailed summary of the firmware state machine, main transitions, and LEDs colors:

States: INITIALIZATION, WAIT, SCAN, SLOW_CHARGE_AND_SCAN, CHARGE

STATE: INITIALIZATION (initializes all modules)

    INIT_DONE → WAIT
    BUTTON_DFU_PRESSED (BTN3) → DFU mode

STATE: WAIT

    BTN1 PRESSED → SCAN
    BTN3 PRESSED→ DFU mode

STATE: SCAN (Blue Left LED ON)

    BLE_DEVICE_FOUND → CHARGE
    BLE_SCAN_TIMEOUT → SLOW_CHARGE_AND_SCAN
    BTN3 PRESSED→ DFU mode

STATE: SLOW_CHARGE_AND_SCAN (White Left LED ON)

    BLE_DEVICE_FOUND → CHARGE
    BLE_SCAN_TIMEOUT → WAIT
    WPT_SCAN_TIMEOUT → WAIT
    BTN3 PRESSED→ DFU mode

STATE: CHARGE (Yellow Left LED ON / middle LED Green when complete)

    CHARGE_COMPLETE → WAIT (middle LED Green ON)
    BTN1 PRESSED → WAIT (stop charge)
    BUTTON_DFU_PRESSED (BTN3) → DFU mode

In summary, after the charger is connected and initialization finishes, the device enters the WAIT state. If BTN1 is pressed, the device enters SCAN mode and the left LED turns blue. If a device is found, the charger transitions to CHARGE and the left LED turns yellow. If no device is found, the charger enters SLOW_CHARGE_AND_SCAN and the left LED turns white. If the device is still not found after that period, the system returns to WAIT (all LEDs off).
If a device is found during slow scan, the system transitions to CHARGE. Once charging is completed, the middle LED turns green and the system returns to WAIT. During charging, if BTN1 is pressed, the device returns to the WAIT state.

## Instructions for Flashing Firmware
1. Install J-Link software: https://www.segger.com/downloads/jlink (when installing, check the box to install legacy USB Driver)
2. Attach J-Link EDU Mini USB cable to Windows computer and in Device Manager confirm that J-Link driver is under USB devices (If it is not there, start J-Link Configurator, right-click on J‐Link EDU Mini, Update FW > Configure > select WinUSB driver > OK)
3. Attach Tag-Connect cable to the charger PCB, open the J-Flash Lite, confirm that Target device is NRF52, and click OK.
4. Click Erase Chip to remove previous FW.
5. Download the two hex files from the Firmware folder.
6. Flash the SoftDevice:
    * Click the “…” button and select 140_nrf52_7.2.0_softdevice.hex 
    * Click Program Device
7. Flash the application:
    * Click the “…” button and select wpt-charger.hex 
    * Click Program Device
      
## Contributors
* [Focus Uy](https://focus.uy/) 
* [Medipace Inc](https://www.medipaceinc.com/) 

## Disclaimer
The contents of this repository are subject to revision. No representation or warranty, express or implied, is provided in relation to the accuracy, correctness, completeness, or reliability of the information, opinions, or conclusions expressed in this repository.

The contents of this repository (the “Materials”) are experimental in nature and should be used with prudence and appropriate caution. Any use is voluntary and at your own risk.

The Materials are made available “as is” by USC (the University of Southern California and its trustees, directors, officers, employees, faculty, students, agents, affiliated organizations and their insurance carriers, if any), its collaborators Med-Ally LLC and Medipace Inc., and any other contributors to this repository (collectively, “Providers”). Providers expressly disclaim all warranties, express or implied, arising in connection with the Materials, or arising out of a course of performance, dealing, or trade usage, including, without limitation, any warranties of title, noninfringement, merchantability, or fitness for a particular purpose.

Any user of the Materials agrees to forever indemnify, defend, and hold harmless Providers from and against, and to waive any and all claims against Providers for any and all claims, suits, demands, liability, loss or damage whatsoever, including attorneys' fees, whether direct or consequential, on account of any loss, injury, death or damage to any person or (including without limitation all agents and employees of Providers and all property owned by, leased to or used by either Providers) or on account of any loss or damages to business or reputations or privacy of any persons, arising in whole or in part in any way from use of the Materials or in any way connected therewith or in any way related thereto.

The Materials, any related materials, and all intellectual property therein, are owned by Providers.

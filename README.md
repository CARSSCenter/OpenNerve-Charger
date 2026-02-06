## About
This repository holds the PCBA files, firmware, and design files for the OpenNerve charger.
The charger is designed to be compatible with the [OpenNerve Implantable Pulse Generator](https://github.com/CARSSCenter/OpenNerve-Implantable-Pulse-Generator) and to inductively charge it at 300kHz.

## Instructions for Flashing Firmware
1. Install J-Link software: https://www.segger.com/downloads/jlink (when installing, check the box to install legacy USB Driver)
2. Attach J-Link EDU Mini USB cable to Windows computer and in Device Manager confirm that J-Link driver is under USB devices (If it is not there, start J-Link Configurator, right-click on J‐Link EDU Mini, Update FW > Configure > select WinUSB driver > OK)
3. Attach Tag-Connect cable to the charger PCB, open the J-Flash Lite, confirm that Target device is NRF52, and click OK.
4. Click Erase Chip to remove previous FW.
5. Extract 2 hex files from the attached ZIP file
6. Flash the SoftDevice:
    * Click the “…” button and select 140_nrf52_7.2.0_softdevice.hex 
    * Click Program Device
7. Flash the application:
    * Click the “…” button and select wpt-charger.hex 
    * Click Program Device
      
## Contributors

## Disclaimer
The contents of this repository are subject to revision. No representation or warranty, express or implied, is provided in relation to the accuracy, correctness, completeness, or reliability of the information, opinions, or conclusions expressed in this repository.

The contents of this repository (the “Materials”) are experimental in nature and should be used with prudence and appropriate caution. Any use is voluntary and at your own risk.

The Materials are made available “as is” by USC (the University of Southern California and its trustees, directors, officers, employees, faculty, students, agents, affiliated organizations and their insurance carriers, if any), its collaborators Med-Ally LLC and Medipace Inc., and any other contributors to this repository (collectively, “Providers”). Providers expressly disclaim all warranties, express or implied, arising in connection with the Materials, or arising out of a course of performance, dealing, or trade usage, including, without limitation, any warranties of title, noninfringement, merchantability, or fitness for a particular purpose.

Any user of the Materials agrees to forever indemnify, defend, and hold harmless Providers from and against, and to waive any and all claims against Providers for any and all claims, suits, demands, liability, loss or damage whatsoever, including attorneys' fees, whether direct or consequential, on account of any loss, injury, death or damage to any person or (including without limitation all agents and employees of Providers and all property owned by, leased to or used by either Providers) or on account of any loss or damages to business or reputations or privacy of any persons, arising in whole or in part in any way from use of the Materials or in any way connected therewith or in any way related thereto.

The Materials, any related materials, and all intellectual property therein, are owned by Providers.

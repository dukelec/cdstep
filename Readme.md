CDSTEP Introduction
=======================================

<img src="doc/cdstep_v4.jpg">

RS-485 wire housing: Molex 5264 (4 pin)

Download this project:
```
git clone --recursive https://github.com/dukelec/cdstep.git
```

## Protocol

CDSTEP is an open-source stepper motor controller that communicates over an RS485 interface.
 - Default baud rate: 115200 bps
 - Maximum speed: 50 Mbps
 - Default address: 0xfe

The underlying protocol is CDBUS, with the following frame format:  
`src, dst, len, [payload], crc_l, crc_h`

Each frame includes a 3-byte header, a variable-length payload, and a 2-byte CRC (identical to Modbus CRC).  
For more information on the CDBUS protocol, please refer to:
 - https://cdbus.org

The payload is encoded using the CDNET protocol. For detailed information, please refer to:
 - https://github.com/dukelec/cdnet
 - https://github.com/dukelec/cdnet/wiki/CDNET-Intro-and-Demo


## Block Diagram

<img src="doc/block_diagram.svg">

## GUI Tool

CDBUS GUI Tool: https://github.com/dukelec/cdbus_gui

After power on, first write 1 to `state`, then write the target position to `tc_pos`, then the stepper motor will rotate.

<img src="doc/cdbus_gui.png">


After modifying the configuration, write 1 to `save_conf` to save the configuration to flash.  
If you need to restore the default configuration, change `magic_code` to another value and save it to flash. Then reapply power.

Logs:

<img src="doc/csa_list_show.png">

Plots:

<img src="doc/plot.png">

Plot details, IAP upgrade, data import/export (including registers, logs, plots).

<img src="doc/iap_export.png">

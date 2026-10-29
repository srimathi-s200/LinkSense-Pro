# LinkSense Pro

**Reliable Wireless Communication and Protocol Diagnostics Using ESP32 and ESP8266**

## Overview

LinkSense Pro is an embedded wireless communication prototype that uses Wi-Fi and UDP to exchange data between an ESP32 and an ESP8266 NodeMCU. It implements application-level packet integrity checks, sequence numbers, acknowledgements, retransmission, and communication performance statistics.

## Features

- UDP communication over Wi-Fi
- CRC-16/CCITT-FALSE packet integrity checking
- Sequence-number-based packet identification
- Acknowledgement and stop-and-wait retransmission
- Packet delivery ratio and retry-overhead measurement
- Round-trip time monitoring

## Hardware

- ESP32 development board — sender and Wi-Fi access point
- ESP8266 NodeMCU — receiver and acknowledgement node

## Protocol

The application packet contains a protocol marker, sequence number, payload, and CRC-16 checksum.

Example: `LS1,121,HELLO,3B5F`

The sender waits for a matching acknowledgement and retries when the acknowledgement timeout expires.

## Test Results

Tests at approximately 3 m and 5 m recorded 3,977 acknowledged packets, zero recorded failures, and one retransmission across 3,978 transmissions. The combined weighted average RTT was approximately 5.60 ms.

These figures apply only to the recorded test windows and environment.

## Limitations

The current measurements do not establish maximum range or guarantee performance in all environments. Sequence-accounting logic, especially out-of-order detection, should be validated against the source code.

## Repository Contents

- `esp32_sender/` — ESP32 sender firmware
- `esp8266_receiver/` — ESP8266 receiver firmware
- `docs/` — detailed architecture, protocol, and test documentation
- `images/` — hardware setup and serial-monitor screenshots

## Technologies

ESP32, ESP8266, Arduino IDE, Wi-Fi, UDP, CRC-16, embedded C/C++.
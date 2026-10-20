# COENICT
## Cost optimized ESP32-S3 based penetration testing training device

This repository is part of an academic study on using MCU-based devices in penetration
testing/red-teaming. COENICT is a prototype of a network implant device, based
on the ESP32-S3 (N16R8) microcontroller. The main purpose of the device is to serve as an educational and training platform, allowing experimentation and further development. 

### Dependencies
 - ESP-IDF ver.5.5.0
 - [trombik/esp_wireguard v0.9.0](https://github.com/trombik/esp_wireguard)
 
### Configuring COENICT
All needed settings are located in a separate sub-menu in the standard ESP-IDF configuration system, started by `idf.py menuconfig` in terminal. In `Custom config` sub-menu everything needed can be filled in. There are two configurable parts - TelegramBot (used as a C2 channel), and WireGuard tunnel to a machine in the red-team infrastructure. The attack machine on the oposite side of the tunnel should be configured as a WireGuard server that accepts connection from the device. 

### Building and running

ESP-IDF ver.5.5.0 should be installed and sourced into the terminal. Target should be set to `esp32s3`. Building follows standard ESP-IDF procedure:

* `idf.py build` - all dependencies are fetched as a managed components during this phase
* `idf.py flash` - flashing the chip with the firmware
* `idf.py monitor` - to check the logs in the terminal

On successful connection, device automatically starts polling the Telegram Bot API and takes responses from the configured bot. Bot commands defined in code:

* `/status` - returns a list of all LwIP interfaces on the device.  
* `/wg` - establishes tunnel to configured endpoint and port
* `/d` - disconnects the tunnel
* `/newssid <ssid_name> <password>` - switches WiFi network
* `/newserver <endpoint> <port>` - switches endpoint


## Repository organization

Source code of the project is located in [natproxy/](natproxy/) folder. Experimental raw data, scripts and other related materials are placed in folders with a `exp_#` prefix. Each `exp_` folder is for a separate experimental setting and materials are published with articles announcing them.

## Experimental settings
### Phase 1 - Testing encrypted tunnel capabilities

Full procedure and results interpretation can be found in [experiment_design.md](exp_0/experiment_design.md) file. All raw results, as well as the scripts used for automating the test procedure, results extraction and plot generation, can be found in [exp_0/](exp_0/) subfolder.


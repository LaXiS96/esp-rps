# ESP remote power switch

Turn on and off PC remotely via HTTP API (over WiFi) by interfacing with front panel headers on the motherboard.

- Partition table is configured for OTA, but OTA update is not yet implemented
- 2x960KB app images, 2MB SPIFFS partition

## TODO
- [ ] Serve static files from SPIFFS
- [ ] Update SPIFFS contents via API
- [ ] mDNS
- [ ] Add MOSFETs and test

## Building on Windows

1. Ensure Python is installed
2.
    ```
    cd ~/esp
    git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git
    cd ESP8266_RTOS_SDK
    git checkout origin/release/v3.4
    python tools/idf_tools.py install
    python tools/idf_tools.py install-python-env
    ```
1. Copy `export.bat` and/or `export.ps1` from https://github.com/espressif/esp-idf to the root of ESP8266_RTOS_SDK
1. Open console, launch export script, change to project directory
1. Use `idf.py`

## References
- https://github.com/espressif/ESP8266_RTOS_SDK
- https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/index.html

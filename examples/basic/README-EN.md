

[中文](./README.md)

# BeShell Demo Project

This is the demo project for [BeShell](https://beshell.become.cool).

[BeShell](https://beshell.become.cool) is an embedded JavaScript framework that allows developers to build embedded applications using only JavaScript.

## Preparation

### Hardware

Prepare an ESP32 device. Any ESP32 development board will work.

### Development Environment

All build and flash operations are performed in the ESP-IDF environment. You need to install ESP-IDF first:

* Standalone installation: [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.4.2/esp32/get-started/index.html)
* **Recommended:** Use the [ESP-IDF Extension for VSCode](vscode:extension/espressif.esp-idf-extension).

## Build and Flash

For the initial project setup, simply run `idf.py build flash` to complete the entire build and flashing process (make sure the device is connected to your PC).

[BeShell](https://beshell.become.cool) supports allocating a dedicated partition on the device's flash to store JavaScript files, which can be executed and imported just like on a PC.

The project's `CMakeLists.txt` provides commands for packaging and flashing JS files:

### Pack JS Scripts

Use the `idf.py pack-js` command to package all files in the `js` directory into an image file `img/js.bin` for flashing.

> * When running `idf.py build` to build the entire project, the JS packaging command will be executed automatically.
> * The source directory (`js`) and image file (`img/js.bin`) can be modified in `CMakeLists.txt`.
> * If the target file already exists and all files in the source directory have not changed, this command will not be executed again.

### Flash JS Scripts

Use the `idf.py flash-js` command to flash the `img/js.bin` file to the JS partition on the device's flash.

The partition start address can be modified in `CMakeLists.txt` and should match the JS partition start address in `partition.csv`. Also, the size of `img/js.bin` must not exceed the JS partition size.

> * When running `idf.py flash` to flash the entire project, the JS partition flashing command will be executed automatically.

## Run JavaScript Examples

[BeShell](https://beshell.become.cool) firmware provides an interactive JS execution environment (`REPL`) supporting serial, websocket, Bluetooth, USB, and more. You can use any serial tool or the online console [BeConsole](https://beconsole.become.cool) to connect and run [BeShell](https://beshell.become.cool) firmware on your ESP32 device, send commands or JS code to the firmware, and receive program output and JS return values.

On startup, the firmware will automatically run `/main.js` from the JS partition (the auto-start JS file path can be modified in `main.cpp`).
Some JS examples are prepared in the `/example` directory. In a serial tool or [BeConsole](https://beconsole.become.cool), enter `run [js file path]`, e.g., `run /example/wifi-ap.js` to run the example. Enter `reboot` to return to the `/main.js` entry point.

## Online File System Access

After connecting to the device with [BeConsole](https://beconsole.become.cool), you can list, access, and manage JS files on the device. BeConsole also integrates the VSCode editor core, allowing you to edit and run JS files on the device online in real time. You can also package all JS files on the device into a zip file for download.

# Project Name: SV Simulator

## Overview

This project is a C-based simulator designed to interact with and manage various modules, including an integrated IEC 61850 Sampled Values (SV) publisher. It utilizes a state machine for robust application flow and leverages the `libiec61850` library for IEC 61850 communication. The project is structured for easy compilation with a `Makefile` supporting both debug and release builds.

## Features

* **Modular Architecture**: Organized into distinct modules (e.g., `Module_Manager`, `State_Machine`, `IPC`, `Logger`, `Ring_Buffer`, `Util`).
* **State Machine**: Manages the application's lifecycle and transitions between states (e.g., `IDLE`, `INITIATION`, `RUNNING`, `STOP`).
* **Integrated SV Publisher**: Includes an IEC 61850 Sampled Values (SV) publisher as a module, allowing programmatic control over SV message generation and transmission on a specified network interface.
* **Logging System**: Features a custom logger for detailed output, especially useful in debug mode.
* **IPC (Inter-Process Communication)**: Connects to a Node.js IPC server for potential external control or data exchange.
* **Build System**: Uses a `Makefile` for streamlined compilation, providing `debug` and `release` targets.

## Project Structure

The project follows a standard C project layout:

```
.
├── BIN/             # Compiled executables will be placed here
├── INC/             # Header files (.h)
├── LIB/
│   └── libiec61850-1.5.1/ # libiec61850 source and build directory
├── OBJ/             # Object files (.o) generated during compilation
└── SRC/             # Source files (.c)
    ├── main.c
    ├── Module_Manager.c
    ├── State_Machine.c
    ├── ipc.c
    ├── logger.c
    ├── Ring_Buffer.c
    ├── util.c
    ├── sv_publisher_module.c # The refactored SV Publisher
    └── 
```

## Prerequisites

Before building the project, ensure you have the following installed:

* **GCC (GNU Compiler Collection)**: Used for compiling C code.
    ```bash
    sudo apt update
    sudo apt install build-essential
    ```
* **libxml2 development headers**: Required for XML parsing functionalities.
    ```bash
    sudo apt install libxml2-dev
    ```
* **OpenSSL development headers**: Required for TLS/SSL functionalities used by `libiec61850`.
    ```bash
    sudo apt install libssl-dev
    ```
* **cJSON development headers**: Used for JSON parsing (if `lcjson` is in your `LDFLAGS`).
    ```bash
    sudo apt install libcjson-dev # or build from source if not available in repos
    ```
* **`libiec61850`**: The project explicitly links against `libiec61850`. The `Makefile` includes logic to build this library if it's not already compiled. It's expected to be located at `./LIB/libiec61850-1.5.1`.

## Building the Project

Follow these steps to build the `sv_simulator` executable:

1.  **Navigate to the project root directory**: This is the directory containing the `Makefile`.

2.  **Build `libiec61850`**: The `Makefile` will attempt to build `libiec61850` if it's not found. However, it's good practice to ensure it's built independently first, especially after cloning the repository.
    ```bash
    cd LIB/libiec61850-1.5.1
    make all
    cd - # Go back to the project root directory
    ```

3.  **Compile the Project**:
    * **Debug Build (with debugging symbols and no optimizations):**
        ```bash
        make debug
        ```
    * **Release Build (optimized for performance):**
        ```bash
        make release
        ```
    The compiled executable, `sv_simulator`, will be located in the `BIN/` directory.

## Running the Simulator

To run the simulator, execute the compiled binary from the project root:

```bash
sudo ./BIN/sv_simulator
```
**Note on `sudo`**: The `sv_publisher` module, which uses raw sockets for SV communication, typically requires root privileges to operate on network interfaces.

## Cleaning the Project

To remove all compiled object files and the executable:

```bash
make clean
```
This command will also clean the build artifacts of `libiec61850`.

---

## Modifying the SV Publisher

The SV Publisher is integrated as a module (`sv_publisher_module.c/h`).

* **Configuration**: The network interface for SV publishing (e.g., "eth0") is currently set during the `SVPublisher_Module_init` call within `State_Machine.c`. You would modify this in `State_Machine.c` or implement a configuration loading mechanism (e.g., from a JSON file using `cJSON`) to make it truly configurable at runtime.
* **Data Generation**: The dummy SV data (`fVal1`, `fVal2`) is generated within `sv_publisher_module.c`. To publish real sensor data, you would modify the `sv_publishing_thread` function to acquire data from your actual sensors or simulation sources.

---

## Troubleshooting

* **`hal_thread.h: No such file or directory`**: This indicates that the compiler cannot find the `libiec61850` header files. Ensure `LIBIEC_HOME` is correctly set in your `Makefile` and that `libiec61850` has been successfully built. Run `make clean` then `make debug` again.
* **`static declaration of ‘state_enter’ follows non-static declaration`**: This means `state_enter` is called before its prototype is known. Ensure the `static bool state_enter(...)` prototype is placed at the top of your `State_Machine.c` file, before any calls to `state_enter`.
* **Linking errors (`undefined reference to ...`)**: This usually means either:
    * A source file containing the definition of the missing symbol is not being compiled or included in `SRC`.
    * A required library (e.g., `libiec61850`, `libxml2`, `libcjson`, `pthread`) is not being linked. Check the `LDFLAGS` in the `Makefile`.
    * The `libiec61850.a` file itself was not built or is in the wrong location. Rebuild `libiec61850` as described in the "Building" section.

Feel free to open an issue or contact the project maintainer if you encounter further issues.

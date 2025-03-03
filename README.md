# Nalu Board Controller

The **Nalu Board Controller** is a C++ library designed to interface with and control the Nalu Board hardware using C++. This library includes utilities for board configuration, logging, and IP address management, providing a clean API to interact with the Nalu system. This is not a full controller, but may be expanded in the future.

## Features

- **Board Management**: Provides functionalities to configure and manage Nalu Boards.
- **Logging**: Built-in logging for diagnostics and monitoring.

## Prerequisites

To build and use the Nalu Board Controller, the following software must be installed:

- **CMake** (version 3.10 or higher)
- **A C++ compiler** (e.g., GCC, Clang)

### Python Dependencies

This project requires the following Python modules:

- `pybind11`: A Python binding generator for C++11.
- `naludaq`: A Python module used for interacting with the nalu hardware.

You can install the required Python dependencies using `pip`:

```bash
pip install pybind11 naludaq
```
or
```bash
pip install -r requirements.txt
```

## Installation

Here’s how you can modify your installation instructions to encourage using the provided `scripts/build.sh` and `scripts/install.sh` scripts for a more streamlined build and installation process:

---

## Installation

### Step 1: Clone the repository

Start by cloning the repository to your local machine:

```bash
git clone https://github.com/jaca230/nalu_board_controller.git
cd nalu_board_controller
```

### Step 2: Build the project

To simplify the build process, we’ve included a `build.sh` script to handle the configuration and compilation for you. Run the following command:

```bash
scripts/build.sh
```

This script will:
- Set up a build directory.
- Run `cmake` to configure the project.
- Compile the project with `make`.

If you want to clean and rebuild the project, you can add the `-o` or `--overwrite` flag to the command:

```bash
scripts/build.sh -o
```

### Step 3: Install the library (optional)

To install the library system-wide, you can use the `install.sh` script, which allows you to specify the installation prefix. By default, it installs the library to `/usr/local`, but you can specify a custom location with the `-p` or `--prefix` option.

To install the library:

```bash
scripts/install.sh
```

To install the library to a custom directory:

```bash
scripts/install.sh -p /your/custom/path
```

This script will:
- Run the build process if needed.
- Install the library into the specified location, defaulting to `/usr/local/include` and `/usr/local/lib`.

If you want to overwrite a previous installation, you can add the `-o` or `--overwrite` flag to the command:

```bash
scripts/install.sh -o
```

---

## Usage

Once installed, you can link your C++ applications with the Nalu Board Controller library.

The `main.cpp` file provided in the example is a C++ program that demonstrates how to interact with the Nalu Board using the `NaluBoardManager` and how to capture UDP packets using the `capture_packets` function.

### Key Components of `main.cpp`

1. **Initialization**:
    - The program initializes the `NaluBoardManager` object, which is responsible for managing communication with the board.
    - It also sets up the configuration parameters (such as channels, trigger mode, and capture settings) and prepares the board for packet capture.
   
2. **Capture Control**:
    - The program starts the capture using `board_manager.start_capture()` and stops it using `board_manager.stop_capture()`. The capture process is tightly controlled within the main flow of execution.

3. **Logging**:
    - Throughout the program, `NaluBoardControllerLogger` is used for logging information and error messages, helping in debugging and tracking the capture process.

### How to Run the Example

In the `example/test` directory, you can use the `run.sh` script to execute the `main.cpp` program. Here's how you can use the `run.sh` script:

### `run.sh` Script Overview

The `run.sh` script allows you to run the compiled `main` executable from the `build/bin` directory. The script provides an option to run the executable in debug mode using `gdb`.

1. **Running without Debugger**:
    - If you simply want to run the program without debugging, you can execute:
    ```bash
    ./scripts/run.sh
    ```

2. **Running with Debugger**:
    - If you want to run the program with `gdb` for debugging, you can use the `--debug` flag:
    ```bash
    ./scripts/run.sh --debug
    ```

To include the example code and instructions for running the program in your README, you can add a section like this:

---

## Example: Running the NaluBoardManager

This section provides an example of how to use the `NaluBoardManager` in a C++ program to initialize the board, configure the capture parameters, and start/stop the capture process.

### Example Implimentation (`main.cpp` stripped down):

```cpp
#include <iostream>
#include <chrono>

#include "nalu_board_controller.h"

int main() {
    try {
        // Step 1: Initialize NaluBoardManager
        NaluBoardParams board_params;
        board_params.model = "HDSoCv1_evalr2";
        board_params.board_ip_port = "192.168.1.59:4660";
        board_params.host_ip_port = "192.168.1.1:4660";
        board_params.config_file = "";
        board_params.clock_file = "";
        board_params.debug = false;

        NaluBoardController board_manager(board_params);

        // Step 2: Initialize the board (This step takes some time)
        board_manager.initialize_board();

        // Step 3: Initialize capture parameters
        int num_channels = 32;
        NaluCaptureParams capture_params = NaluCaptureParamsWrapper(num_channels).get_capture_params();

        capture_params.target_ip_port = "192.168.1.1:12345";
        capture_params.assign_dac_values = false;
        capture_params.windows = 4;
        capture_params.lookback = 4;
        capture_params.write_after_trig = 4;
        capture_params.trigger_mode = "ext";
        capture_params.lookback_mode = "";

        // Manually set channels and their trigger/dac values
        for (int i = 0; i < num_channels; ++i) {
            NaluChannelInfo channel_info;
            channel_info.trigger_value = 0;
            channel_info.dac_value = 0;

            capture_params.channels[i] = channel_info;
        }

        // Step 4: Start the capture
        board_manager.start_capture(capture_params);

        // Capture data for 10 seconds
        std::this_thread::sleep_for(std::chrono::seconds(10));

        // Step 5: Stop capture
        board_manager.stop_capture();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}


```

## License

This project is licensed under the [MIT License](LICENSE).

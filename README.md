# Nalu Board Controller

`nalu_board_controller` is a C++ wrapper around the Python `naludaq` control stack. It is intended to be consumed both as a standalone project and as a dependency from larger MIDAS/frontend builds.

## What changed

- CPM-friendly CMake/package layout
- install/export support via `find_package(nalu_board_controller)`
- mirrored `include/` and `src/` layout
- config structs split into separate headers for reflection-friendly use
- direct `spdlog` usage instead of the old logger shim
- window level control is now part of `CaptureConfig`
- standalone example apps live under `apps/examples/`
- app-specific run and screening scripts live with each app

## Build

### Standalone

```bash
./scripts/build.sh
```

This configures with CMake and builds the library in `build/`.

### Example Apps

```bash
./apps/examples/manual_capture/scripts/build.sh
./apps/examples/manual_capture/scripts/run.sh
./apps/examples/packet_listener/scripts/build.sh
./apps/examples/packet_listener/scripts/run.sh
```

The root project builds only the library. Example apps are standalone mini-projects under `apps/examples/`.

### Install

```bash
./scripts/install.sh -p /your/prefix
```

The install exports a CMake package under:

```text
<prefix>/lib/cmake/nalu_board_controller
```

## Consume from CMake

### As an installed package

```cmake
find_package(nalu_board_controller REQUIRED)
target_link_libraries(your_target PRIVATE nalu_board_controller::nalu_board_controller)
```

### Via CPM / add_subdirectory style

This project is now structured so a parent build can bring it in directly and link:

```cmake
target_link_libraries(your_target PRIVATE nalu_board_controller::nalu_board_controller)
```

## Headers

All headers now live under `include/nalu_board_controller/...`, including internal implementation headers. The tree mirrors `src/`:

```text
include/nalu_board_controller/config
include/nalu_board_controller/controller
include/nalu_board_controller/logging
include/nalu_board_controller/python
include/nalu_board_controller/runtime
include/nalu_board_controller/types
```

Example usage:

```cpp
#include <nalu_board_controller/config/board_config.h>
#include <nalu_board_controller/controller/controller.h>

nalu_board_controller::BoardConfig board_config;
nalu_board_controller::Controller controller(board_config);
```

## Layout

```text
include/nalu_board_controller
src/controller
src/logging
src/runtime
src/python
src/types
apps
apps/examples/manual_capture
apps/examples/packet_listener
scripts
```

## Apps

The `apps/examples/` tree is example/application code, not part of the installed library package. Keep app-specific runtime assets and Python dependencies there.

`manual_capture`

- executable: `apps/examples/manual_capture/build/bin/manual_capture`
- config: [apps/examples/manual_capture/config.json](/home/pioneer/packages/software/nalu_board_controller/apps/examples/manual_capture/config.json:1)
- local Python requirements: [apps/examples/manual_capture/requirements.txt](/home/pioneer/packages/software/nalu_board_controller/apps/examples/manual_capture/requirements.txt:1)
- Python setup script: [apps/examples/manual_capture/scripts/environment/setup_venv.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/manual_capture/scripts/environment/setup_venv.sh:1)
- Python activation script: [apps/examples/manual_capture/scripts/environment/activate_venv.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/manual_capture/scripts/environment/activate_venv.sh:1)
- build script: [apps/examples/manual_capture/scripts/build.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/manual_capture/scripts/build.sh:1)
- run script: [apps/examples/manual_capture/scripts/run.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/manual_capture/scripts/run.sh:1)
- screening scripts: [apps/examples/manual_capture/scripts/screening/start.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/manual_capture/scripts/screening/start.sh:1) and [stop.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/manual_capture/scripts/screening/stop.sh:1)

`packet_listener`

- executable: `apps/examples/packet_listener/build/bin/packet_listener`
- config: [apps/examples/packet_listener/config.json](/home/pioneer/packages/software/nalu_board_controller/apps/examples/packet_listener/config.json:1)
- build script: [apps/examples/packet_listener/scripts/build.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/packet_listener/scripts/build.sh:1)
- run script: [apps/examples/packet_listener/scripts/run.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/packet_listener/scripts/run.sh:1)
- screening scripts: [apps/examples/packet_listener/scripts/screening/start.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/packet_listener/scripts/screening/start.sh:1) and [stop.sh](/home/pioneer/packages/software/nalu_board_controller/apps/examples/packet_listener/scripts/screening/stop.sh:1)

Top-level dispatcher:

```bash
./apps/examples/manual_capture/scripts/build.sh
./apps/examples/manual_capture/scripts/environment/setup_venv.sh
./scripts/run.sh manual_capture
./apps/examples/packet_listener/scripts/build.sh
./scripts/run.sh packet_listener
```

## Capture Config

`CaptureConfig` is now composed from smaller structs:

```cpp
nalu_board_controller::CaptureConfig config;
config.readout_window.windows = 8;
config.readout_window.lookback = 8;
config.trigger.mode = "self";
config.window_level_control.configure = true;
config.window_level_control.enabled = true;
```

Relevant headers:

```text
nalu_board_controller/config/board_config.h
nalu_board_controller/config/capture_config.h
nalu_board_controller/config/capture_config_builder.h
nalu_board_controller/config/channel_config.h
nalu_board_controller/config/readout_window_config.h
nalu_board_controller/config/trigger_config.h
nalu_board_controller/config/window_level_control_config.h
```

Behavior:

- `configure = false`: leave WLC untouched
- `configure = true` and `enabled = true|false`: write the `wlc_on` register
- `reinitialize_after_change = true`: re-run board startup after the register change, matching the workflow used in the reference notebooks

This is aimed primarily at HDSoCv1/HDSoCv2-style boards where the register is available. If the underlying board/register path does not support WLC, the operation will fail loudly instead of silently pretending it succeeded.

For `manual_capture`, the executable now reads its settings from JSON instead of hardcoding them in `main.cpp`. Edit [apps/examples/manual_capture/config.json](/home/pioneer/packages/software/nalu_board_controller/apps/examples/manual_capture/config.json:1) to change board, trigger, readout-window, channel, and window-level-control settings. Its HDSoCv1 clock file now lives under `apps/examples/manual_capture/resources/`, alongside the rest of the app assets.

For `packet_listener`, edit [apps/examples/packet_listener/config.json](/home/pioneer/packages/software/nalu_board_controller/apps/examples/packet_listener/config.json:1) to change the UDP bind address, port, and packet parser settings. The listener prints parsed packet information in real time without event bundling.

## Python/runtime requirements

The library embeds Python and talks to `naludaq` at runtime, but this repo now treats the Python environment as an application concern.

For the bundled `manual_capture` app:

```bash
./apps/examples/manual_capture/scripts/environment/setup_venv.sh
source ./apps/examples/manual_capture/scripts/environment/activate_venv.sh
./apps/examples/manual_capture/scripts/run.sh
```

`run.sh` already handles the app-local environment automatically. `activate_venv.sh` is only for interactive shell work when you want to inspect or run Python commands manually inside the same environment.

## License

MIT. See [LICENSE](LICENSE).

# IEC61850_TEST_TOOL
A simulation tool for IEC 61850 Sampled Values (SV) and GOOSE communication.

## Setup
1. Install dependencies: `libiec61850-dev`, `libxml2-dev`.
2. Run `make` to build the project.
3. Execute `./BIN/iec61850_TEST_TOOL` to run the tool.

## Files
- `CONFIG_Files/config.xml`: Configuration file for SV/GOOSE settings.
- `CONFIG_Files/scenario.xml`: Scenario file for simulation data.
- `simulation_log.txt`: Simulation log output.
- `error_report.txt`: Error report output.

## Usage
The tool initializes SV and GOOSE publishers/receivers, sends data based on `scenario.xml`, and logs results.

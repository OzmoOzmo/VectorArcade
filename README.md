# Asteroids ESP32

This PlatformIO Arduino project runs 3 vector games on the original ESP32. The built-in 8-bit DAC renderer is selected by default and outputs X on GPIO25 (DAC1) and Y on GPIO26 (DAC2). These are analog outputs for compatible XY display hardware, typically an Oscilloscope
althought there is an option to display on an LCD video - it has been tested on various Oscilloscopes and on the built in display of a Weemos Esp32 with built in 0.96 OLED display.

There are 3 games - two written from scratch (Lunar Lander and Millle Command) and one a port of a Astroids.
There are several methods of imputs - Joystick is preferred - but you can use a rotary controller.

Ambrose Clarke
2026


## Build and Upload

Build and upload the firmware and SPIFFS filesystem as separate steps:

Use Platformio to automate the process in the usual way - or manually as follows:


```sh
pio run
pio run -t upload
pio run -t uploadfs
```

The WAV files must be in the project's `data/` directory. PlatformIO packages that directory into a SPIFFS image; `uploadfs` writes the image to the board. Run it after the first firmware upload and again whenever files in `data/` change. Uploading the firmware alone does not upload the WAV files.

If PlatformIO does not detect the board's serial port, specify it for both uploads, for example:

```sh
pio run -t upload --upload-port COM3
pio run -t uploadfs --upload-port COM3
```

Open the serial monitor at 115200 baud for frame and memory statistics. Audio startup logs indicate whether WAV files were found and loaded from SPIFFS.

### Joystick and DAC Calibration

Build and flash the diagnostics firmware with `pio run -e calibration -t upload
--upload-port COM3`, then open the serial monitor at 115200 baud. It reports the
configured joystick, button, and DAC pins; current ADC readings; center readings;
and the minimum/maximum ADC values observed since boot. Move the stick to its
physical left, right, up, and down stops and note the reported raw ranges.
Horizontal stick movement resizes the DAC test square's width; vertical movement
resizes its height. The square starts small and is constrained to DAC codes
64-192 on either axis. Serial output includes approximate voltages using the
nominal 3.3 V DAC range (the actual DAC transfer curve is not perfectly linear).
ADC values within 32 counts of either rail are reported in `rail_x`/`rail_y`
and excluded from travel ranges and square adjustment. Repeated rail counts
suggest checking the joystick signal, power, and ground wiring. Fire and thrust
states are also printed. The diagnostics environment is separate from the
normal firmware environment.

## Renderer and Hardware Configuration

Renderer selection and peripheral pin assignments are in [src/HardwareConfig.h](src/HardwareConfig.h). Set `ASTEROIDS_RENDERER_MODE` to `ASTEROIDS_RENDERER_DAC` (default), `ASTEROIDS_RENDERER_SPI_DAC`, or `ASTEROIDS_RENDERER_OLED`.

The attached 30-pin ESP32 board exposes GPIO25 (DAC1) and GPIO26 (DAC2), used by the default vector renderer for X and Y. The analog joystick's logical X axis reads GPIO39 and logical Y reads GPIO36; both are ADC1 input-only pins. The joystick axes are not inverted in software. Fire and thrust remain on GPIO14 and GPIO16.

The built-in DAC renderer scales both coordinate outputs to 90% around the DAC
midpoint to reduce the displayed vector size without shifting its center. Adjust
`kDacOutputScalePercent` in `src/HardwareConfig.h` to change this output range.

Audio is enabled and routed to an external I2S DAC/amplifier: BCLK on GPIO4, WS/LRCLK on GPIO5, and data out on GPIO13. These pins are exposed on the board. The OLED is not selected; GPIO4 and GPIO5 are therefore available for I2S instead of the onboard display. Do not connect an external circuit that pulls GPIO5 low during reset, as it is an ESP32 strapping pin. Upload the SPIFFS filesystem with `pio run -t uploadfs` to install the WAV files.

Sound effects are used in all three games. Missile Command plays the fire effect
when a defender launches and a large bang whenever an explosion starts. Lunar Lander repeats
the thrust effect while the engine is burning, plays a large bang on a crash,
and plays the extra-ship cue after a safe landing.

The built-in ESP32 DAC renderer runs its timer at 40 kHz (500 ticks at the
20 MHz timer clock). It uses `kDacPixelsPerUpdate` for game frames and
`kDacPixelsPerUpdateMenu` for main-menu frames. Larger strides improve frame
throughput but make drawn lines less dense. Both settings and the timer interval
are in [src/HardwareConfig.h](src/HardwareConfig.h).

### CRT Screensaver

After 60 seconds without joystick or button input, the arcade pauses any active
game and replaces the entire display with a bouncing VECTOR ARCADE title and a
drifting, rotating wireframe diamond. Joystick input, fire, or B wakes the screen.
Controls must return to neutral before gameplay or menu navigation resumes, so
the wake-up input does not fire or select a game. The active game is preserved.

Set `HardwareConfig::kScreenSaverTimeoutSeconds` in
[src/HardwareConfig.h](src/HardwareConfig.h) to change the timeout; 0 disables it.
The animation reduces static exposure but does not guarantee protection against
CRT burn-in. Keep display intensity low, especially without hardware blanking.

### CPU Frequency

`HardwareConfig::kCpuSpeedPercent` is relative to the ESP32's maximum supported
240 MHz clock. Its current value is 200, which requests 480 MHz but is capped at
240 MHz; the installed Arduino framework does not support an overclock beyond
that limit.
Requests are rounded down to supported 80/160/240 MHz values, with a minimum of
80 MHz to keep the APB peripheral clock at 80 MHz. Startup reports the requested
percentage and actual CPU/APB clocks. The SPI clock setting remains unchanged.

### Experimental Buffered SPI Output

Build the dual-core buffered MCP4822 renderer with:

```sh
pio run -e spi-buffered
pio run -e spi-buffered -t upload --upload-port COM12
```

Alternatively, select `ASTEROIDS_RENDERER_SPI_BUFFERED` in
[src/HardwareConfig.h](src/HardwareConfig.h). The normal and existing SPI renderers
are unchanged. The restored arcade entry point supports all four renderer modes.

[src/Rendering/SPIRendererDMA.cpp](src/Rendering/SPIRendererDMA.cpp) retains the
requested filename but does not use hardware DMA: the MCP4822 requires a separate
CS pulse for each 16-bit channel command and an LDAC pulse after each XY pair.
A continuous SPI DMA transfer cannot supply that framing on its own.

Core 0 prepares prepacked samples into two bounded 128-sample blocks; core 1 sends
them using polling SPI with the bus acquired once. Shared endpoints are not
written twice. Laser-state-only changes at an unchanged position are retained,
as are short-line endpoints and dots. Set `HardwareConfig::kSpiBufferedLineStepsPerDacUpdate`
in [src/HardwareConfig.h](src/HardwareConfig.h) to control the stride (currently 16).
Larger values are faster but coarser; smaller values draw more intermediate points.
Output has no per-point timer notification, retains a 5 us settling
delay, and yields one FreeRTOS tick per completed frame. Audio is not required.

The existing `DIAG` serial report gives completed scanout FPS and error count.
Actual speed and visual quality must be checked on hardware; this is not a
guaranteed flicker fix. The embedded arcade tests include sample ordering and
deduplication checks, which can be built without flashing with:

```sh
pio test -e test-arcade --without-uploading --without-testing
```

### KY-040 Rotary Encoder Wiring

Connect the encoder module by its printed pin labels:

| Encoder pin | ESP32 connection |
| --- | --- |
| `CLK` | GPIO19 |
| `DT` | GPIO18 |
| `SW` | GPIO16 (thrust input) |
| `+` | 3V3 |
| `GND` | ESP32 GND |

The encoder rotation inputs use internal pull-ups. The `SW` pushbutton is active-low and is read as the thrust button; GPIO16 is pulled high internally and the button should connect it to GND when pressed. Do not hold the GPIO15 fire button low while resetting or powering up the ESP32, since GPIO15 is a boot-strapping pin.

To print built-in DAC direct-register throughput at startup, set `ASTEROIDS_DAC_BENCHMARK` to `1` in `src/HardwareConfig.h`, rebuild, upload, and read the samples-per-second results from the serial monitor. Leave it at `0` for normal game startup.
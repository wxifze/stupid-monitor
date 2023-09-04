# stupid-monitor
Stupid-like-a-fox system monitor. It reads data from different APIs. It draws it on the screen. That's it. 

<p align="center">
  <img src="https://github.com/wxifze/stupid-monitor/assets/95285485/43eb0535-87a8-45f1-84cb-58437577e9a6" width=500>
  <br>
  <i>Photo after running <code>stress -c 3 -m 50 -t 20</code></i>
</p>


Top to bottom, left to right: CPU usage, CPU temperature, fan speed, RAM temperature, RAM usage, network upload, network download, uptime, disk read, disk write. `kawaii` is my hostname.


# Description
Project contains source of three programs: 
* `monitor` - runs on the linux machine, collects statistics, renders graphics;
* `uart_to_ssd1306` - runs on Arduino board, initializes display, passes data from the host;
* `pbm_to_header` - converts [PBM](https://netpbm.sourceforge.net/doc/pbm.html) image into a C header for error message in `uart_to_ssd1306`.

Currently it's in the state of a Proof of Concept. Statistics are gathered from API points specific for my hardware configuration and it isn't likely to run on any other machine without modification of at least `stats.c`.

# Hardware
* ATmega328p-based Arduino board with USB to UART chip: Uno, Nano and whatnot
* 128 by 64 pixel OLED display with ssd1306 controller

# Software
Host: `linux`. \
Tools: `make`, `gcc`, `avr-gcc`, `avrdude`, optionally `picocom` for board debugging.

# How to run
In the project's current state for anyone who can find it useful, makefiles in each programs directories should be self-explanatory.

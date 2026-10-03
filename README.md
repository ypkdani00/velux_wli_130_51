# VELUX WLI 130 51 — Zigbee IR bridge (ESP32-C6)

Repository: <https://github.com/ypkdani00/velux_wli_130_51>

A battery-powered bridge that lets **Home Assistant** open, stop and close
the windows and blinds wired to one or more **VELUX WLI 130 51** wall keypads.
It speaks **Zigbee** to Home Assistant (ZHA, or any Zigbee coordinator) and
**infrared** to the keypads, replacing the WLR 100 remote control. Built on a
**DFRobot FireBeetle 2 ESP32-C6**; a switch turns **Wi-Fi** on for setup, the
web UI and firmware updates, so day to day the radio that costs power stays
off and the battery lasts months.

## Contents

- [What it does](#what-it-does)
- [Hardware](#hardware)
  - [Wiring diagram](#wiring-diagram)
  - [Pins](#pins)
  - [Bill of materials](#bill-of-materials)
- [Quick start](#quick-start)
- [How it works](#how-it-works)
  - [Two modes, one switch](#two-modes-one-switch)
  - [Status LED and BOOT button](#status-led-and-boot-button)
  - [Zigbee and Home Assistant](#zigbee-and-home-assistant)
  - [The web UI](#the-web-ui)
  - [Running on a battery](#running-on-a-battery)
- [Configuration (`src/Config.h`)](#configuration-srcconfigh)
- [IR protocol](#ir-protocol)
- [HTTP API](#http-api)
- [Firmware updates and flashing](#firmware-updates-and-flashing)
- [Crashes](#crashes)
- [Troubleshooting](#troubleshooting)
- [Project layout](#project-layout)

## What it does

The **WLI 130 51** is a VELUX wall keypad with a built-in IR receiver, wired
to a **WLC 100** control unit that drives up to 3 chain actuators (window,
blind, awning). It is normally operated with the **WLR 100** infrared remote.
It is an old, one-way, IR-only system — not io-homecontrol / KLF — that VELUX
discontinued years ago.

This firmware re-implements the WLR 100's IR protocol, so the board can:

- **Transmit** open / stop / close commands to any number of WLI 130 keypads
  (each keypad only reacts to its own 10-bit security code).
- Show up in Home Assistant over **Zigbee** as window coverings, one per
  motor plus an "all" cover per keypad, with a **position estimated** from the
  travel time, a battery level and an Identify button.
- Run on a **battery** for months: Zigbee as a sleepy end device, light sleep
  between radio wake-ups, Wi-Fi off unless you switch it on.
- With the Wi-Fi switch ON, serve a mobile-friendly **web UI** (controls, log,
  battery history, Wi-Fi and Zigbee setup, firmware update) and an **HTTP
  API**.
- Protect the actuators: a command in the opposite direction while a motor
  may still be travelling is preceded by a **STOP**, and a STOP never waits.

The IR protocol itself (timing, frame layout, and the 4-bit checksum, which
public sources considered unsolved) was reverse-engineered from real captured
frames — see the comments in `src/VeluxIR.h`.

## Hardware

### Wiring diagram

```
                         +------------------------------------+
   Li-ion cell           |   DFRobot FireBeetle 2 ESP32-C6    |
   3.7 V / 2500 mAh ---> |   BAT connector (charger onboard)  |
                         |                                    |
   USB-C (charging, ---> |   USB-C                            |
   flashing)             |                                    |
                         |   3V3   o---+-----------+          |
                         |   GND   o---|---+-------|---+      |
                         |             |   |       |   |      |
                         |   GPIO22 o--|---|--+    |   |      |   (MOSI)
                         |   GPIO21 o--|---|--|-+  |   |      |   (MISO)
                         |             |   |  | |  |   |      |
                         |   GPIO15  onboard status LED (D13) |
                         |   GPIO9   onboard BOOT button (D9) |
                         |   GPIO0   battery sense, via the   |
                         |           board's own 1:2 divider  |
                         +-------------|---|--|-|--|---|------+
                                       |   |  | |  |   |
              Gravity IR transmitter   |   |  | |  |   |   Gravity self-locking
              (DFR0095)                |   |  | |  |   |   switch (DFR0423)
              +------------------+     |   |  | |  |   |   +--------------------+
              |  V   (3V3)   ----+-----+   |  | |  +---|---+-- V   (3V3)        |
              |  G   (GND)   ----+---------+  | |      +---+-- G   (GND)        |
              |  S   (signal) ---+------------+ |          |                     |
              |        |         |              |          |  S   (signal) ------+--+
              |      IR LED      |              +----------+----------------------+ |
              +------------------+                                                 |
                                                             (GPIO21 = switch)  <---+
```

In plain words — everything is powered from the board's **3V3** pin and
shares **GND**; only the two signal wires matter:

| module | pin on the module | goes to |
|---|---|---|
| Gravity IR transmitter (DFR0095) | **V** | 3V3 |
| | **G** | GND |
| | **S** (signal) | **GPIO22** (`MOSI` on the board) |
| Gravity self-locking switch (DFR0423) | **V** | 3V3 |
| | **G** | GND |
| | **S** (signal) | **GPIO21** (`MISO` on the board) |
| Li-ion cell | the board's battery connector | check the + / − marking before plugging it in |

The IR LED must **see every keypad** it drives (a few metres, line of sight);
aim it at the keypads' IR windows. The status LED, the BOOT button and the
battery measurement are all on the board: nothing to wire.

### Pins

| function | GPIO | name on the board | notes |
|---|---|---|---|
| IR transmitter signal | **22** | `MOSI` | the module lights its LED on a HIGH signal (`kIrActiveLow = false`) |
| Wi-Fi mode switch | **21** | `MISO` | latched = HIGH = Wi-Fi on; unplugged = off |
| status LED | 15 | `D13` (onboard) | strapping pin: leave it alone |
| BOOT button | 9 | `D9` (onboard) | hold 3 s to forget the Zigbee network; strapping pin |
| battery voltage | 0 | — (onboard divider 1:2) | nothing to wire |

The numbers in the firmware are **GPIO numbers**; the board's silkscreen
mostly prints its own names (`A1`–`A4`, `D2`, `D13`, `MOSI`, `MISO`, `SDA`…).
GPIO8, GPIO9 and GPIO15 are strapping pins: do not connect anything that
could hold them high or low at power-up. To use other pins change them in
`src/Config.h` (section 3).

### Bill of materials

| qty | part | part no. | notes |
|---|---|---|---|
| 1 | **DFRobot FireBeetle 2 ESP32-C6** | DFR1075 | Zigbee + Wi-Fi 6 + BLE (BLE unused), Li-ion charger onboard, ~16–36 µA in deep sleep |
| 1 | **Gravity: Digital IR Transmitter Module** | DFR0095 | the IR LED and its driver |
| 1 | **Gravity: Digital Self-Locking Switch** | DFR0423 | the Wi-Fi mode switch (any latching switch to 3V3 works) |
| 1 | **Li-ion / LiPo cell, 3.7 V** | e.g. 505573, 2500 mAh | with the connector the board expects (check the polarity!) — or a 18650 + holder |
| 6 | Dupont jumper wires, female–female | — | or Gravity 3-pin cables with a breakout; 3 per module |
| 1 | USB-C data cable | — | charging and the first flash (a charge-only cable shows no COM port) |
| 1 | Zigbee coordinator | e.g. any ZHA / Zigbee2MQTT stick | the one you already have for Home Assistant |
| — | VELUX **WLI 130 51** keypad(s) and **WLC 100** | — | the existing installation (needs no change) |
| opt. | enclosure, double-sided tape | — | the IR LED must still see the keypads |

## Quick start

1. Install **VS Code** with the **PlatformIO IDE** extension (it brings its
   own Python) and wait until it reports PlatformIO Core as installed.
2. Open this folder and edit **`src/Config.h`** — at least the keypads'
   security codes (section 1), and the travel times of your windows and blinds
   (section 4).
3. Plug the board in over USB and run, from a PowerShell prompt in this folder:
   ```powershell
   .\export_release.ps1 -Upload -Port COM4
   ```
   (or the VS Code task "VELUX: build + flash over USB"). The **first build
   takes 20–30 minutes**: it compiles ESP-IDF with light sleep, Zigbee and
   without Bluetooth/Thread. Later builds take about a minute. If the board
   does not start by itself after flashing, press **RST** once.
4. **Pair with Home Assistant:**
   - put the Wi-Fi switch **ON** (the board opens its own network
     `VELUX-WLI-Setup`, password `velux1234`; join it, open
     <http://192.168.4.1>, and pick your home Wi-Fi under **Wi-Fi**);
   - in ZHA choose **Add device** (permit join on), then in the web UI go to
     **Settings → Zigbee → Start pairing**. The LED flashes once a second
     until the board has joined, then once every 5 seconds;
   - flip the switch **OFF**: from now on the board runs in Zigbee-only mode.
5. In Home Assistant you now have eight covers (see "Zigbee and Home
   Assistant" for their names). **Switch "USB log" off** (Settings) before
   running on the battery.

## How it works

### Two modes, one switch

| switch | radios | what works |
|---|---|---|
| **OFF** | Zigbee only, Wi-Fi off | commands from Home Assistant over Zigbee — the low-power mode |
| **ON** | Zigbee **and** Wi-Fi | the same, plus the web UI, the HTTP API and firmware updates |

Flipping the switch restarts the board into the other mode (a few seconds;
Zigbee re-joins by itself). Boot always runs at full speed (160 MHz).

### Status LED and BOOT button

| LED (onboard, GPIO15) | meaning |
|---|---|
| steady | booting |
| one 30 ms flash every **1 s** | Zigbee looking for a network (pairing or rejoining) |
| one 30 ms flash every **5 s** | joined and waiting (or not paired yet) |
| pulsing every 500 ms for 10 s | Home Assistant's **Identify** button (to tell the board apart) |

Holding the **BOOT** button for 3 seconds makes the board **forget the Zigbee
network** (it restarts unpaired; pair again from the web UI).

### Zigbee and Home Assistant

The board joins as a battery-powered "sleepy" end device named **VELUX WLI
130 IR** and exposes **window coverings**, one per motor plus one per keypad
for "all three" (endpoint = `kZigbeeFirstEndpoint` + keypad × 4 + motor):

| endpoint | with the default `kPanels[]` |
|---|---|
| 10, 11, 12 | Window 1, 2, 3 |
| 13 | All windows |
| 14, 15, 16 | Blind 1, 2, 3 |
| 17 | All blinds |
| 30 | USB log (On/Off switch, see below) |

- **USB log switch.** Endpoint 30 is an On/Off switch for the same setting as
  Settings → USB log, so the console can be turned on or off without Wi-Fi.
  The two are one stored setting and follow each other: change it in the web
  UI and the switch in Home Assistant follows; change the switch and the
  setting is stored and **the board restarts** to apply it (the console and
  light sleep are set up at boot; the position estimate is lost with the
  restart). The board does not push its state to Home Assistant after a
  restart (an explicit report of the On/Off attribute aborts inside the
  stack), so if the switch shows the wrong state, press the other position
  first, then the one you want. Adding this endpoint to an already paired
  board needs a
  **Reconfigure** of the device in ZHA (or a new pairing) before the switch
  shows up.
- **Pairing** is started from the web UI (**Settings → Zigbee → Start
  pairing**) with the coordinator in permit-join mode, and works only while
  the Wi-Fi switch is ON. **Forget network** (or BOOT for 3 s) undoes it.
  Pairing again **without removing the device from ZHA** keeps the entities
  and their entity IDs.
- **Open / close / stop** send the matching IR frame. **Set position**: below
  50% opens, 50% and above closes — the IR protocol only knows "all the way".
- **Position estimate.** The IR protocol never reports back, so the board
  **estimates** the position from the travel time (`kTravelTimeWindowMs` =
  23 s, `kTravelTimeBlindMs` = 25 s): an actuator moves at constant speed, an
  open / close starts the estimate, a STOP freezes it, the end of travel
  stops it there. Home Assistant gets it **every second while a motor runs**;
  "All windows / All blinds" have a position of their own and follow only the
  commands sent to them: moving one window by itself leaves "All windows"
  alone, while an "all" command moves the three windows too. The estimate is
  stored when the motors stop and restored after a restart (a reboot changes
  nothing in Home Assistant); only the very first start is a guess (50%), and
  after one full run it is right again. Every full run corrects any drift. The reported value stays between 1% and 99%, never exactly 0% or
  100%: Home Assistant greys out the open (close) button of a cover it
  believes fully open (closed), and opening or closing again must always be
  possible.
- **Types.** Windows are announced as *roller shades* and blinds as *exterior
  roller shades* (Zigbee has no window type; its "shutter" is tilt-only and
  ZHA would drop the up/down controls). For a window icon set the entity's
  **Show as → Window**, or in `configuration.yaml`:
  ```yaml
  homeassistant:
    customize:
      cover.velux_wli_130_ir_cover:   # your entity id
        device_class: window
  ```
- **Names.** Zigbee has no per-endpoint names, so ZHA calls the covers
  "Cover", "Cover 2"… — rename them once (they keep the names across
  restarts and re-pairing). `zha/velux_wli130.py` is an experimental ZHA quirk
  that sets the names automatically (install steps at its top; not
  guaranteed on every ZHA version).
- **Identify** makes the status LED pulse for 10 s.
- **Battery:** measured once a minute, sent to the coordinator when joined
  and then every hour (`kZigbeeBatteryReportMs`).
- **Speed.** A sleepy device only hears its parent when it asks. The board
  asks every `kZigbeePollMs` (1 s) and polls fast for 30 s after joining and
  after every command, so a command reaches the IR LED in well under a
  second.
- **Priority.** Zigbee and IR come first: commands are sent by their own task
  (priority 21) and the Zigbee stack's task runs at 20, both above lwIP (18,
  the web server's network stack) and the main loop (1), so a slow web
  request never holds a command up. A **STOP** is more urgent still: it goes
  first in the queue, drops any open / close for the same motor still waiting,
  cuts short the pause of a reversal in progress, and skips the short wait
  after a frame. Only the frame already being transmitted (~0.2 s) has to
  finish.
- **Anti-jam.** VELUX chain actuators lock up (red LED on the keypad) if they
  get a command in the *opposite* direction while still travelling. If one
  arrives within the travel time of the last open / close (plus
  `kTravelMarginMs`), the board sends a **STOP** first, waits
  `kAutoStopPauseMs` (1 s) and then sends the new direction. If a keypad does
  get stuck, press its physical STOP button.

### The web UI

Only while the Wi-Fi switch is ON: <http://velux.local> (or the IP shown in
Settings → Device). Four tabs, light or dark following your phone/PC; the
header shows the connection state and the battery level, and the page polls
the board gently (every 2.5 s while you use it, rarely when idle, not at all
while hidden: every request wakes the radio).

- **Control** — one card per keypad from `kPanels[]`, with open / stop /
  close per motor and an "All three" row; below, **Send a raw frame** (any
  24-bit frame, in hex or binary) with the exact frames each button sends.
  The anti-jam cooldown does not apply to raw frames.
- **Log** — two sections, each with **Clear** and **Export JSON / TXT** (the
  file is built in the browser):
  - **Frame log** — the commands: the last 32 (`kLogEntries`), in RAM (lost
    at a restart): transmitted IR frames decoded (action, motor, security
    code, checksum) and commands received over Zigbee (**ZB**), each with its
    time (`dd/mm hh:mm:ss`, Rome time by default).
  - **Battery history** — one reading every 2 hours (and at each restart) for
    7 days, kept in flash across restarts, with the drain in %/day.
  - **Crash log** — the restarts caused by a crash, in flash (see "Crashes").
- **Wi-Fi** — connection status, **Scan networks**, **Join manually** (hidden
  networks) and the **Access point password** of the setup network.
- **Settings** — **Firmware update**, **Zigbee** (Start pairing / Forget
  network), **IR LED test** (keeps the LED lit for 10 s: check it with a phone
  camera), **USB log** on/off, and **Device** diagnostics (battery, Zigbee
  state, Wi-Fi signal, power management, CPU clock, free heap, uptime, last
  boot reason, addresses) with a **Reboot** button.

The setup access point `VELUX-WLI-Setup` only opens while the board cannot
reach your Wi-Fi (never configured, or the router has been gone for 2
minutes); once it joins, the AP closes. An access point cannot sleep (~100 mA).

### Running on a battery

Day to day the board runs in **Zigbee-only mode** (switch OFF): a sleepy end
device that wakes its radio once a second to ask for commands, and sleeps in
between.

| what | setting (`Config.h`) | effect |
|---|---|---|
| Wi-Fi off unless the switch is ON | `kWifiSwitchPin` | the biggest saving |
| Sleepy Zigbee, polling every 1 s | `kZigbeePollMs` | longer = less power, slower commands |
| Light sleep | `kEnablePowerManagement` | the chip naps between radio wake-ups, with the clock at 80 MHz (below that the Zigbee link breaks, see `kCpuFreqIdleMinMhz`) |
| Bluetooth and Thread compiled out | `platformio.ini` | smaller image, nothing running |
| IR LED powered only by the signal | — | no idle draw from the transmitter driver |
| Battery read once a minute | — | 7-day history in flash |
| Status LED: 30 ms flash every 5 s | `kLedFlashMs`, `kLedIdlePeriodMs` | ~0.01 mA |

**Switch "USB log" off** (Settings): while it is on, light sleep stays off so
the USB console works, and the board draws ~15 mA — about a week of battery.
With it off, Settings → Device → **Power management** reads `light sleep +
DFS 80 MHz` (`160 MHz` with Wi-Fi on). A side effect: with light sleep the
chip's USB port switches off, so the COM port disappears (see
Troubleshooting).

**What to expect** (an *estimate* from catalogue figures — the board has no
current sensor; the module draws are the unknown to measure):

| Zigbee poll interval | average draw | 2500 mAh lasts |
|---|---|---|
| 5 s | ~0.25 mA | about a year |
| **1 s** (default) | ~0.5 mA | **4–6 months** |
| 300 ms | ~1 mA | 2–3 months |

The easiest real measurement is **Log → Battery history**, which gives the
drain in %/day after a few hours.

## Configuration (`src/Config.h`)

Everything you are likely to change lives in one file, in numbered sections:

1. **Keypads** — `kPanels[]`: name, 10-digit security code (the keypad's DIP
   switches, left to right, ON = `1`; try the reversed string if nothing
   responds), the three motor labels, the "all" label and the `kind`
   (`kCoverWindow` / `kCoverBlind`: sets the Zigbee covering type and the
   travel time). Add or remove rows freely.
2. **Wi-Fi** — `kWifiSsid` / `kWifiPassword` (empty = configure from the web
   UI), the setup AP name and password, `kHostname`, the NTP server and
   `kTimezone` (POSIX TZ string; Rome by default), `kLogEntries`, Wi-Fi
   transmit power, power save (`WIFI_PS_MAX_MODEM`, listen interval 5 =
   ~0.5 s) and the reconnect watchdog.
3. **Pins** — IR (`kIrTxPin`, `kIrActiveLow`), the switch, the status LED
   (and its timings, including the Identify pulse) and the BOOT button.
4. **Travel times and anti-jam** — `kTravelTimeWindowMs` (23 s),
   `kTravelTimeBlindMs` (25 s), `kTravelMarginMs`, `kAutoStopPauseMs`.
   **Measure your actuators with a stopwatch and enter the real values**: they
   drive both the anti-jam protection and the position estimate.
5. **Power saving** — the CPU clock (`kCpuFreqBootMhz` 160, `kCpuFreqWifiMhz`
   160, `kCpuFreqMhz` 80, `kCpuFreqIdleMinMhz` 80; boot always runs at full
   speed, then Wi-Fi mode stays at the maximum and Zigbee-only floats between
   the minimum and `kCpuFreqMhz` - keep the minimum at 80: at 40 MHz the Zigbee
   link breaks and no command arrives), `kEnablePowerManagement`, and the USB
   log default.
6. **Battery** — pin, divider, calibration (measure the cell with a
   multimeter and set `kBatteryCalibration` to *real ÷ shown*), capacity.
7. **Zigbee** — first endpoint, manufacturer ("VELUX") and model ("WLI 130
   IR"), sleepy or not, position-report limits, poll interval, battery report
   interval, and the **task priorities** (`kCmdTaskPriority` 21,
   `kZigbeeTaskPriority` 20).
8. **Reliability** — `kLowHeapBytes`, `kCrashLoopLimit`, `kCrashStableMs`
   (see "Crashes").

## IR protocol

24-bit frames: action (3 bits: open `001`, close `011`, stop `101`), motor
(3 bits, one-hot: `001`, `010`, `100`, all `111`), "set" (4 bits, always 0),
the keypad's security code (10 bits) and a 4-bit checksum. Each frame is a
~32.1 kHz carrier modulated with mark / space pairs per bit (about 39 ms) and
**sent twice** with a 19 ms gap. The frames are generated by the chip's RMT
peripheral, independent of the CPU clock and of whatever the Wi-Fi is doing.
The checksum for "all three motors" was obtained by extrapolation and never
seen on a real frame; set values other than 0 ("all sets") cannot be
generated. Example frames for the default panel `0100000000`:

| motor | open | stop | close |
|---|---|---|---|
| 1 (Window 1) | `0x241008` | `0xA41002` | `0x64100D` |
| 2 | `0x28100D` | `0xA81007` | `0x681008` |
| 3 | `0x301007` | `0xB0100D` | `0x701002` |
| all three | `0x3C1002` | `0xBC1008` | `0x7C1007` |

## HTTP API

Available while the Wi-Fi switch is ON; no authentication — fine on a home
network, never port-forward it.

| method | endpoint | notes |
|---|---|---|
| GET | `/api/status` | version, Wi-Fi/AP state, signal, CPU clock, heap, uptime, boot reason, MAC, panels, the frame log with `logTimes`, power management, `zigbee` state, `zigbeePaired`, `zigbeePairing`, `usbLog` / `usbLogSaved`, `irTest`, `battery` |
| POST | `/api/send?panel=P&motor=M&action=A` | `P` = index into `kPanels[]`; `M` 0–2 = motor 1/2/3, `3` = all; `A` 0–2 = open, stop, close |
| POST | `/api/sendhex?v=0x241008` | a raw frame (hex, or 24 characters of `0`/`1`) |
| GET | `/api/frames` | the frames each button sends, per keypad |
| GET | `/api/battery/history` | the week of readings, oldest first: `{"t","up","mv","pct","boot"}` |
| POST | `/api/battery/history/clear` | deletes it |
| POST | `/api/log/clear` | clears the frame log |
| GET | `/api/crashlog` | the crash log, oldest first, and `safeMode` |
| POST | `/api/crashlog/clear` | clears it (and ends the safe mode) |
| GET | `/api/prevlog` | the log of the run before this one (`boot`, and `lines` with `ms` since that run started and `t` the text), oldest first |
| POST | `/api/crashtest?kind=exception|panic|terminate` | provokes a fault on purpose, to check the crash log |
| POST | `/api/wifi` | `{"ssid":"…","pass":"…"}`, then reboots |
| GET | `/api/wifiscan` | nearby networks |
| POST | `/api/appassword` | `{"pass":"…"}` for the setup AP (8–63 chars, or empty = default), then reboots |
| POST | `/api/usblog` | `{"enabled":true\|false}`, applied at the next restart |
| POST | `/api/irtest` | `{"enabled":true\|false}`: IR LED lit for testing, switches off after 10 s |
| POST | `/api/zigbeepair` | start the search for a Zigbee network |
| POST | `/api/zigbeeforget` | leave the Zigbee network |
| POST | `/api/update` | multipart upload of a compiled `.bin`; flashes it and reboots |
| POST | `/api/reboot` | reboots the board |

```bash
curl -X POST "http://velux.local/api/send?panel=0&motor=0&action=0"   # open window 1
curl -X POST "http://velux.local/api/send?panel=1&motor=3&action=2"   # close all blinds
```

## Firmware updates and flashing

`.\export_release.ps1` builds with PlatformIO and leaves **one file**,
`..\build\velux_wli_130_51_c6.bin`, next to the project folder; with
`-Upload [-Port COMx]` it also flashes the board over USB. `-OutDir` and
`-Jobs N` change the output folder and the number of compile jobs.

- **First flash: USB.** It also writes the partition table (two 1.875 MB app
  slots and the Zigbee partitions). If the upload does not start, hold
  **BOOT**, tap **RST**, release **BOOT**.
- **Later updates: no cable.** Switch ON → web UI → **Settings → Firmware
  update** → choose the `.bin`. Saved Wi-Fi, the Zigbee pairing and every
  setting are kept (separate flash partitions). If the upload is interrupted
  the board keeps running the old firmware.
- **ArduinoOTA** (hostname `velux`, port 3232) is running too.
- Build from PowerShell / VS Code, not Git Bash (ESP-IDF's tools refuse to
  run under MSYS). Only the first build, and any change to `platformio.ini`,
  takes minutes.

## Crashes

A restart caused by a crash - a CPU exception (null pointer, bad access), a
watchdog, a brownout, a stack overflow - is **recorded and shown** instead of
just happening:

- ESP-IDF writes a **core dump** to the `coredump` partition before it reboots.
  At the next boot the firmware reads its summary - the **task** that crashed,
  the **program counter**, the **cause**, the return addresses found on its
  stack - keeps it in flash (the last 10, across restarts) and erases the dump.
  They appear in **Log → Crash log** (Clear, Export JSON / TXT), in the USB log
  and in the frame log (`!!  Restarted after a crash`).
- **C++ exceptions** thrown in the main loop or in a Zigbee command are
  **caught and logged** (`!!  Exception caught: …`) and the board carries on.
  One that nothing catches ends in a panic, recorded with its text.
- **Low memory**: free heap below `kLowHeapBytes` for 30 s restarts the board on
  purpose (recorded) before it runs out of memory altogether.
- **Crash loop → safe mode.** After `kCrashLoopLimit` (3) crashes in a row the
  board starts in **safe mode**: Wi-Fi forced on whatever the switch says,
  Zigbee off, the status LED pulsing like Identify - so you can read the log and
  update the firmware instead of watching it reboot for ever. **Clear** the
  crash log (Log tab) to leave safe mode; a run of `kCrashStableMs` (10 min)
  without a crash resets the count by itself.

**The log of the previous run.** The frame log lives in RAM and is gone at every
restart - including the one that follows switching the Wi-Fi switch back on after
a session without Wi-Fi, which has neither web UI nor (with the USB log off)
serial. So every log line, plus a few trace lines (boot and reset reason, mode,
power configuration, Zigbee joined / lost, every command received from the
network, a sign of life every 30 s), is also kept in **RTC memory**, which
survives software restarts and crashes (not a power loss). `GET /api/prevlog`
returns the run before the current one: to find out what the board did in
Zigbee-only mode, switch Wi-Fi on afterwards and read it. The last 96 lines are
kept.

**Decoding an entry.** The addresses (`pc`, `ra`, the backtrace) are code
locations in the firmware. Decode them on the PC with the `.elf` file of
**the same build** (`.pio\build\firebeetle2_c6\firmware.elf` - keep a copy with
every release you flash):

```powershell
& "$env:USERPROFILE\.platformio\packages\toolchain-riscv32-esp\bin\riscv32-esp-elf-addr2line.exe" `
  -pfiaC -e .pio\build\firebeetle2_c6\firmware.elf 0x42015628 0x4200c288
```

prints the function and source line of each address. The backtrace is taken
from the raw stack (RISC-V cannot unwind on the chip), so a few entries may be
stale values - the first ones are the most reliable.

**Testing it.** With the Wi-Fi switch ON, `POST /api/crashtest?kind=exception`
throws an exception (caught, logged, the board carries on), `kind=panic`
writes to address 0 (a real CPU exception) and `kind=terminate` throws one
nobody catches; the last two restart the board and leave an entry. Three
panics in a row start the safe mode.

## Troubleshooting

- **The COM port is gone / the upload cannot open the port.** With "USB log"
  off the chip sleeps and its USB port switches off, as intended for the
  battery. Switch the log on from Settings, or put the chip in download mode
  (hold **BOOT**, tap **RST**, release **BOOT**), or update over Wi-Fi.
- **After flashing, nothing happens.** The reset over USB does not always get
  through: press **RST** once.
- **The upload says the port is busy.** Close the serial monitor first.
- **Zigbee: the board does not rejoin / commands arrive late.** Check the
  coordinator is on; the log (USB) shows the Zigbee state every 10 s. A board
  reports `not paired` until it has joined once.
- **Nothing moves.** Check the IR LED with a phone camera (Settings → IR LED
  test); the keypad's own LED lights for about a second on a frame with the
  *wrong* security code, and not at all on a malformed signal. Try the
  security code reversed.
- **A keypad shows a red LED.** The anti-jam protection was bypassed (a raw
  frame, the wall buttons…): press its physical STOP button.

## Project layout

- `src/Config.h` — **start here**: every setting you are likely to change
- `src/main.cpp` — the firmware: Zigbee, Wi-Fi, web server, OTA, IR
  transmission, anti-jam, position estimate, power management, battery
- `src/VeluxIR.h` — the protocol: frame layout, checksum, encode / decode
- `src/WebUI.h` — the web page (single file, no external resources); after
  editing it run `node tools/build_webui_gz.js` (the build does it for you)
- `src/WebUI_gz.h` — generated from `WebUI.h`, do not edit
- `platformio.ini`, `partitions_c6_zigbee.csv` — the build: Zigbee end
  device, light sleep, flash layout
- `export_release.ps1` — build, flash and export the `.bin`
- `tools/` — `build_webui_gz.js` (regenerates `WebUI_gz.h`, needs only
  Node.js) and `pio_prebuild.py` (runs before every build; also swaps in a
  placeholder when Windows Smart App Control blocks `littlefs-python`, which
  firmware builds do not need)
- `.vscode/` — build tasks (Ctrl+Shift+B) and the parallel-jobs setting
- `zha/velux_wli130.py` — optional ZHA quirk that names the covers
- `LICENSE`

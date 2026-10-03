#pragma once
#include <Arduino.h>
#include <WiFi.h>   // for wifi_power_t / WIFI_POWER_* below

// ===========================================================================
//  USER CONFIGURATION
// ===========================================================================
//  Everything you are likely to need to change when adapting this project to
//  a different installation lives in this one file: the security code of
//  each keypad, the Wi-Fi credentials, the IR pins, the safety delay, the
//  power-saving options, the battery monitor and Zigbee. main.cpp
//  and the other files should not need any editing.
//
//  Board: DFRobot FireBeetle 2 ESP32-C6. Zigbee all the time, Wi-Fi (web UI,
//  HTTP API, OTA) only while the mode switch is ON - see sections 3 and 7.
//  Built with PlatformIO (env firebeetle2_c6).
// ===========================================================================

// ----- 1) KEYPADS / SECURITY CODES -----------------------------------------
// One entry per WLI 130 keypad you want to control.
//
//   security : the keypad's 10 internal DIP switches, read LEFT TO RIGHT,
//              switch ON = '1'. THIS IS THE MOST IMPORTANT VALUE TO GET
//              RIGHT WHEN ADAPTING THIS FIRMWARE TO A NEW INSTALLATION.
//              If nothing responds, try the string reversed: the mapping
//              between the first physical switch and the first bit of the
//              frame is not documented anywhere. (Firmware up to v1.10 could
//              also read the code from the original WLR 100 remote through
//              an IR receiver; that was dropped in 2.0, transmit only now.)
//   motor[]  : labels for the 3 actuators wired to that keypad (M1, M2, M3
//              on the WLC 100), shown in the web UI and the Zigbee log.
//   all      : label for "all three at once" (Zigbee: the 4th cover).
//   kind     : what the motors move, so Zigbee reports a fitting covering
//              type: kCoverWindow (roof windows) or kCoverBlind (roller
//              shutters / blinds). See section 7 for how the names and
//              types show up in Home Assistant.
//
// Add or remove rows freely; everything else adapts automatically.
enum CoverKind : uint8_t { kCoverWindow, kCoverBlind };

struct Panel {
  const char* name;
  const char* security;
  const char* motor[3];
  const char* all;
  CoverKind   kind;
};

static const Panel kPanels[] = {
  {"Windows", "0100000000",               // 2nd switch ON
   {"Window 1", "Window 2", "Window 3"}, "All windows", kCoverWindow},
  {"Blinds",  "1001000100",               // detected via WLR 100
   {"Blind 1", "Blind 2", "Blind 3"},    "All blinds",  kCoverBlind},
};
static const uint8_t kPanelCount = sizeof(kPanels) / sizeof(kPanels[0]);

// ----- 2) WI-FI ------------------------------------------------------------
// Home network the board should join. Leave BOTH empty ("") to skip this and
// configure Wi-Fi entirely from the web UI instead (see kApSsid/kApPassword
// below): on first boot, or whenever it cannot join a network, the board
// opens its own access point where you can enter your SSID/password from a
// phone or laptop (or use the web UI's "Scan networks" button to pick one
// and just type the password), and that choice is then kept in flash (NVS)
// across reboots. Filling these in here makes the board join your network on
// every boot without needing that setup step, and overrides any value saved
// from the web UI.
static const char* kWifiSsid     = "";
static const char* kWifiPassword = "";

// Fallback access point, used only while kWifiSsid/kWifiPassword above are
// empty AND no network has been configured yet from the web UI.
static const char* kApSsid     = "VELUX-WLI-Setup";
static const char* kApPassword = "velux1234";

// Network name of the board: reachable at http://<kHostname>.local
static const char* kHostname = "velux";

// Internet time server (Wi-Fi mode). The clock dates the log lines (USB and
// Log tab) and the battery history (Log tab, second section). The chip has no
// battery-backed clock, so the time is kept in flash - at every restart and
// hourly - and carries on from there in Zigbee-only mode, where there is no
// internet: such times show a "~" in the log, since they drift a little.
// Empty ("") = don't sync.
static const char* kNtpServer = "pool.ntp.org";

// Time zone of the log lines, as a POSIX TZ string. Rome / Central Europe,
// with daylight saving: "CET-1CEST,M3.5.0,M10.5.0/3". UTC: "UTC0".
static const char* kTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";

// How many commands the frame log (Log tab) keeps: the last ones, in RAM -
// lost at a restart. About 150 bytes per entry; the status page grows with it.
static const uint8_t kLogEntries = 16;


// Wi-Fi radio transmit power. Lowering it reduces the radio's peak current
// draw, which helps if the board resets/disconnects right when sending an IR
// command: a classic BROWNOUT symptom (see resetReasonName() in main.cpp) is
// the combined peak current of the Wi-Fi radio and the IR LED exceeding what
// a marginal power supply can deliver, tripping the chip's brownout
// detector. This trades a bit of Wi-Fi range for headroom. If you
// confirm brownouts via the serial "Reset reason" line, check the battery
// and its wiring - that is the actual fix; this setting is just a
// software-side mitigation.
//
// All values accepted by the Arduino core's wifi_power_t, strongest to
// weakest - pick any of these names:
//   WIFI_POWER_19_5dBm  (max)
//   WIFI_POWER_19dBm
//   WIFI_POWER_18_5dBm
//   WIFI_POWER_17dBm
//   WIFI_POWER_15dBm    <- default here
//   WIFI_POWER_13dBm
//   WIFI_POWER_11dBm
//   WIFI_POWER_8_5dBm
//   WIFI_POWER_7dBm
//   WIFI_POWER_5dBm
//   WIFI_POWER_2dBm
//   WIFI_POWER_MINUS_1dBm  (min)
// Each step down roughly halves the transmitted power (~3dB) and, with it,
// the radio's peak current draw. Below about WIFI_POWER_8_5dBm expect
// noticeably shorter Wi-Fi range - fine next to your own router, likely too
// weak from another room.
static const wifi_power_t kWifiTxPower = WIFI_POWER_11dBm;

// Wi-Fi power save: how much the radio is allowed to doze off between the
// router's beacons (sent every ~102ms). Lowers average, not peak, power draw,
// at the cost of latency: a request only gets through once the radio wakes up.
//   WIFI_PS_NONE      - radio always on: ~100mA, instant response. Mains only.
//   WIFI_PS_MIN_MODEM - wakes at every DTIM beacon (usually every ~100-300ms).
//   WIFI_PS_MAX_MODEM - wakes every kWifiListenInterval beacons: least power.
//                       Default here, for battery operation.
// With MAX_MODEM the radio can miss broadcast traffic (ARP, mDNS) sent while
// it sleeps - more often the longer kWifiListenInterval is. The ESP-IDF
// re-announces the board's address periodically, so this mostly goes
// unnoticed, but if the board becomes hard to reach, or velux.local stops
// resolving, lower kWifiListenInterval or step back to WIFI_PS_MIN_MODEM.
// Tip: give the board a fixed address (DHCP reservation on the router) and
// use the IP rather than velux.local in bookmarks - name lookup
// (mDNS) is the part that suffers most from a sleeping radio.
// Only applies while connected to your router: the setup access point keeps
// the radio fully on whenever it is running.
static const wifi_ps_type_t kWifiPowerSave = WIFI_PS_MAX_MODEM;

// With WIFI_PS_MAX_MODEM: wake up every this many beacons (~102ms each).
//    3 - ESP-IDF default, ~0.3s
//    5 - ~0.5s: a command waits up to ~0.5s (0.25s on average). Default
//        here - a balance between response time and battery: the radio's
//        wake-ups are the biggest cost left once light sleep works.
//   10 - ~1s: ~0.2mA less than 5, commands wait up to ~1s.
static const uint16_t kWifiListenInterval = 5;

// Restricts the radio to a 20MHz-wide Wi-Fi channel (HT20) instead of the
// default 40MHz (HT40). Lower theoretical throughput (~72 vs ~150 Mbps),
// which is irrelevant here - the whole API is small JSON responses - in
// exchange for a bit less RF front-end power. Safe to always leave on: unlike
// kWifiPowerSave, this has no effect on responsiveness/latency, only on
// unused headroom bandwidth.
static const bool kWifiNarrowBandwidth = true;

// Wi-Fi Reconnect Watchdog: monitors connection health without blocking the
// loop (the HTTP server and Zigbee stay responsive).
//   - kWifiWatchdogIntervalMs: interval between status checks.
//   - kWifiReconnectGraceMs  : wait this long after a drop before trying reconnect.
//   - kWifiFullResetGraceMs  : if still disconnected, do a clean disconnect + begin.
//   - kWifiApFallbackGraceMs : if home network is down > this time, enable emergency
//                              AP so controls remain reachable locally.
static const uint32_t kWifiWatchdogIntervalMs = 5000;
static const uint32_t kWifiReconnectGraceMs   = 15000;
static const uint32_t kWifiFullResetGraceMs   = 60000;
static const uint32_t kWifiApFallbackGraceMs  = 120000;

// ----- 3) HARDWARE PINS -----------------------------------------------------
//   Gravity Digital IR Transmitter (DFR0095): signal -> GPIO22 (MOSI),
//     VCC -> 3V3, GND
//   Gravity Digital Self-Locking Switch (DFR0423): signal -> GPIO21 (MISO),
//     VCC -> 3V3, GND
//   Battery: the board's own 1:2 divider on GPIO0 (see section 6)
//   Onboard LED GPIO15 (status), BOOT button GPIO9
// Avoid GPIO8/GPIO9/GPIO15 for anything that could hold them at boot:
// they are strapping pins.
//
// kWifiSwitchPin: a latching ("self-locking") switch that turns Wi-Fi on.
//   ON (pin HIGH)  -> Wi-Fi up: web UI, HTTP API, OTA updates (+ Zigbee).
//   OFF (pin LOW)  -> Zigbee only, Wi-Fi radio off: the low-power mode.
//   Flipping it restarts the board into the other mode (a couple of
//   seconds; Zigbee re-attaches on its own). Unplugged = OFF.
// kStatusLedPin: steady while booting; a short flash every kLedPairingPeriodMs
//   while Zigbee looks for a network (pairing); every kLedIdlePeriodMs once
//   joined and waiting (or not paired). The same with Wi-Fi on.
// kZigbeeResetPin: hold it LOW for 3s (the BOOT button) to leave the Zigbee
//   network and pair again.
static const uint16_t kIrTxPin        = 22;
// Signal level that lights the IR LED: false = HIGH (the usual case, and the
// DFR0095's). If the LED sits lit all the time, or the keypads ignore
// everything, this is worth flipping - after checking the wiring.
static const bool     kIrActiveLow    = false;
static const uint8_t  kWifiSwitchPin  = 21;
static const uint8_t  kStatusLedPin   = 15;
static const uint32_t kLedFlashMs         = 30;     // length of one flash
static const uint32_t kLedPairingPeriodMs = 1000;
static const uint32_t kLedIdlePeriodMs    = 5000;
// Home Assistant's "Identify" button: the LED pulses to tell the board apart.
static const uint32_t kLedIdentifyMs         = 10000;   // how long
static const uint32_t kLedIdentifyPeriodMs   = 500;     // one pulse every...
static const uint32_t kLedIdentifyFlashMs    = 250;     // ...lit for this long
static const uint8_t  kZigbeeResetPin = 9;

// ----- 4) TRAVEL TIME / ANTI-JAM SAFETY / AUTO-STOP -------------------------
// VELUX chain actuators have a jam/overload protection: if they receive a
// command in the OPPOSITE direction while still travelling (or fail to
// complete a full run in the expected time), the WLC 100 control unit enters
// an error state (solid red LED on the WLI 130 keypad) and stays locked until
// it receives a STOP command.
//
// To prevent this, the firmware features automatic anti-jam protection:
// If an open/close command is received in the OPPOSITE direction while the
// actuator is still travelling, the firmware automatically transmits a STOP
// frame first, waits kAutoStopPauseMs for motor inertia to settle, and only
// then transmits the new direction.
//
// "Still travelling" = within the full travel time (fully closed <-> fully
// open) of that kind of motor (kPanels[].kind, section 1) plus
// kTravelMarginMs. Measure the real times with a stopwatch and enter them
// here: too short and a reversal can reach a moving actuator unprotected,
// too long and a reversal is preceded by a needless STOP and a 1 s pause.
static const uint32_t kTravelTimeWindowMs = 23000;   // windows: 23 s
static const uint32_t kTravelTimeBlindMs  = 25000;   // blinds:  25 s
static const uint32_t kTravelMarginMs     = 2000;    // safety margin on top
static const uint16_t kAutoStopPauseMs    = 1000;

// ----- 5) POWER SAVING ------------------------------------------------------
// CPU clock speed (the chip's maximum is 160MHz). Boot always runs at
// kCpuFreqBootMhz, full speed, so Wi-Fi, Zigbee and the web server come up as
// fast as possible; once the board is up the clock depends on the mode:
//  - Wi-Fi switch ON: kCpuFreqWifiMhz, fixed - the web UI, OTA updates and the
//    Wi-Fi stack get all the speed (and the board is on mains / being worked
//    on anyway).
//  - Zigbee only (the battery mode): the clock floats between
//    kCpuFreqIdleMinMhz at rest and kCpuFreqMhz while a Zigbee packet or an IR
//    frame is being handled, then goes back down. 40MHz is the crystal clock,
//    the lowest the chip supports. The IR frames are timed by the RMT
//    peripheral, independent of the CPU clock.
// The floating clock needs power management (kEnablePowerManagement) and
// the USB log off; with the USB log on the clock stays at kCpuFreqMhz.
static const uint8_t kCpuFreqBootMhz    = 160;
static const uint8_t kCpuFreqWifiMhz    = 160;
static const uint8_t kCpuFreqMhz        = 80;
static const uint8_t kCpuFreqIdleMinMhz = 40;


// Automatic light sleep + DFS (dynamic frequency scaling), via the ESP-IDF
// power management API. When nothing is happening the chip drops its clock
// to kCpuFreqIdleMinMhz (Zigbee-only mode) and, between radio wake-ups, goes
// into light sleep while staying connected to the Zigbee parent / the
// router. This is the setting that decides between "days" and "months" on
// a battery.
// platformio.ini builds the ESP-IDF libraries with it enabled
// (custom_sdkconfig); Settings -> Device shows whether it's running.
//
// The main loop doesn't poll: it sleeps until a request, an OTA packet or a
// Zigbee command actually arrives, with or without light sleep.
static const bool kEnablePowerManagement = true;

// USB log (115200 baud on the board's USB port). On or off from the web UI
// (Settings -> USB log, applied after a restart); kUsbLogDefault is the
// value until it has been changed there. Switch it OFF for battery
// operation. While it is on:
//   - at boot the board waits up to kUsbLogWaitMs for a serial monitor to
//     open the port, so the boot messages aren't lost. The USB port is the
//     chip's own: it disappears and comes back with every restart, and
//     whatever is printed before the PC reopens it is gone;
//   - power management stays off (overrides kEnablePowerManagement): light
//     sleep switches the chip's USB port off and the console goes silent;
//   - a status line every kUsbLogStatusMs (mode, Zigbee, battery, heap).
static const bool     kUsbLogDefault  = true;
static const uint32_t kUsbLogWaitMs   = 5000;
static const uint32_t kUsbLogStatusMs = 10000;

// ----- 6) BATTERY MONITOR ---------------------------------------------------
// Measures the single-cell Li-ion/LiPo battery (3.7V nominal, 4.2V full - here
// a 505573 2500mAh / 9.25Wh pack) and reports the charge level in the web UI
// header, in /api/status and to the Zigbee coordinator (Home Assistant shows
// it as the device's battery level).
//
// The battery connector already goes to GPIO0 through the board's own 1:2
// divider - nothing to wire; only the ratio matters here. A full cell puts
// 2.1V on the pin, well inside the ADC's range.
//
// The percentage is derived from the voltage through a typical Li-ion
// discharge curve (table in main.cpp), so it is an estimate: it reads high
// while the battery is charging, and a little low under heavy load.
//
// kBatteryCalibration: resistor tolerance and the ADC's chip-to-chip spread
// add up to a few percent of error, and the onboard divider is
// high-impedance - calibrating once is worth it. Measure the battery with a
// multimeter, compare with the voltage shown under Settings -> Device, and
// set this to real / shown  (e.g. 4.02 / 3.95 = 1.018).
static const uint8_t  kBatteryPin         = 0;
static const uint32_t kBatteryRTopOhm     = 100000;  // 1:2 onboard divider
static const uint32_t kBatteryRBottomOhm  = 100000;
static const float    kBatteryCalibration = 1.000f;
static const uint16_t kBatteryCapacityMah = 2500;    // only for the "mAh left" estimate

// ----- 7) ZIGBEE ------------------------------------------------------------
// The board joins your Zigbee network (Home Assistant ZHA or Zigbee2MQTT) as
// a battery-powered "sleepy" end device and shows up as window coverings,
// one per motor plus one per keypad for "all three":
//
//   endpoint kZigbeeFirstEndpoint + panel*4 + motor
//     motor 0..2 = M1..M3 of that keypad, 3 = all three at once
//   e.g. with the two keypads above: 10-12 Window 1-3, 13 All windows,
//                                    14-16 Blind 1-3,  17 All blinds
//
// Open / close / stop (and "set position": below 50% = open, otherwise
// close) send the same IR frames as the web UI, anti-jam logic included.
// Positions are optimistic - the VELUX IR protocol never reports back.
//
// Names and icons in Home Assistant: Zigbee has no per-endpoint name, so ZHA
// calls every cover "Cover", "Cover 2"... The ZHA quirk in zha/ renames them
// after kPanels (motor[] / all). Covering type, shown in ZHA's diagnostics:
// "roller shade" for kCoverWindow (Zigbee has no window type) and "exterior
// roller shade" for kCoverBlind; both give up/down controls and the "shade"
// device class. "shutter" isn't used: ZHA treats it as tilt-only and drops
// the up/down controls. For a window icon: HA entity settings -> "Show as"
// -> Window. After changing a kind, re-run "Reconfigure" on the device in
// ZHA (or re-pair).
//
// Pairing: put your coordinator in "permit join" and power the board up; it
// joins by itself. To pair again with another network, hold BOOT
// (kZigbeeResetPin) for 3 seconds.
//
// kZigbeePollMs: a sleepy device only hears its parent when it asks. This
// is how often it asks, i.e. the longest a Zigbee command waits. Each poll
// costs a few ms of radio: 1000 = commands within ~1s, a fraction of a mA.
static const uint8_t  kZigbeeFirstEndpoint = 10;
// Home Assistant shows manufacturer + model: "VELUX WLI 130 IR". Changing
// them needs a new pairing (ZHA reads them once, when the device joins).
static const char*    kZigbeeManufacturer  = "VELUX";
static const char*    kZigbeeModel         = "WLI 130 IR";
// Sleepy end device: the radio is off between polls (battery operation).
// false = radio always on, commands instant, ~20 mA more: only to test.
static const bool     kZigbeeSleepy        = true;
// Position reported to Home Assistant (Zigbee lift percentage: 0 = open,
// 100 = closed; HA shows 100 - it). The IR protocol never reports back, so
// the position is ESTIMATED from the travel time (kTravelTime*Ms, section 4):
// an actuator moves at a constant speed, an open / close starts it, a STOP
// freezes it, an end of travel stops the estimate there. It is sent every
// kZigbeePositionReportMs while a motor runs. At boot the position is unknown
// (kZigbeeStartLiftPct, halfway) until the first full run corrects it.
// The reported value is kept between kZigbeePosMinPct and kZigbeePosMaxPct,
// never exactly 0 or 100: at the ends HA greys out the open (close) button,
// and opening or closing again must always be possible.
static const uint8_t  kZigbeeStartLiftPct   = 50;
static const uint8_t  kZigbeePosMinPct      = 1;
static const uint8_t  kZigbeePosMaxPct      = 99;
static const uint32_t kZigbeePositionReportMs = 1000;
// How often the board asks its parent for waiting commands: the longest a
// Zigbee command waits, and the main battery cost (a few ms of radio each
// time). The stack's own default is ~5 s - the delay before this was set.
static const uint32_t kZigbeePollMs        = 1000;
// After joining and after every command the stack polls fast ("turbo poll")
// for this long: Home Assistant's pairing conversation and a second tap on a
// button then don't wait a whole poll interval per message.
static const uint32_t kZigbeeFastPollHoldMs = 30000;
// How long boot waits for the Zigbee stack to rejoin its network (the loop,
// and with it the web UI, only starts after that). Rejoining takes a few
// seconds, a full channel scan ~8 s. If the network isn't there the stack
// keeps trying in the background: the LED blinks until it's joined.
static const uint32_t kZigbeeBeginWaitMs   = 10000;
static const uint32_t kZigbeeBatteryReportMs = 60UL * 60 * 1000;   // battery % to the coordinator, hourly

// FreeRTOS task priorities (higher = more urgent; the chip has one core).
// Zigbee and the IR commands come first: the Zigbee stack's task and the task
// that sends the commands both run above lwIP's (18, the web server's
// network stack) and the Arduino loop (1: web server, battery, LED), so
// neither makes a Zigbee command wait. Only the Wi-Fi driver (23) and the
// system timer (22) stay above them.
static const uint8_t  kZigbeeTaskPriority = 20;
static const uint8_t  kCmdTaskPriority    = 21;

// ===========================================================================
//  END USER CONFIGURATION
// ===========================================================================

// Bump this string on every change: it is shown in the web page header and
// printed to the serial console at boot, so you always know which build is
// actually running on the board.
static const char* kVersion = "2.0";

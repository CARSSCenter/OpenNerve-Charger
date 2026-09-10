# WPT Charger — Firmware Implementation Report

**Hardware:** Hornet WPT Charger PCBA
**MCU:** nRF52840 (via nRF5 SDK)
**Repo path:** `Firmware/Source-Code/`

---

## 1. Overview

The charger firmware manages the complete wireless power transfer (WPT) charging session from user button press to battery full. It communicates with the IPG entirely through BLE advertisement scanning — the IPG broadcasts its charging status, thermistor readings, and GPIO state in a Manufacturer-Specific Data (MSD) payload every second while charging, and the charger uses that data to make power and thermal decisions.

The firmware is built on an **event-driven architecture (EDA)** framework. The application layer, BLE service, WPT service, and PMC service each run their own state machines and communicate exclusively by sending events to each other's ports. No state machine inspects another's internal state directly.

---

## 2. Hardware Overview

### 2.1 WPT Controller — LTC4125

The LTC4125 is the wireless power transmitter IC. In normal operation, LTC4125 autonomously runs an "Optimum Power Search (OPS)" algorithm to find the power level at which the receiver (IPG) is best coupled, using the reflected impedance from the secondary coil to determine best coupling. However, this firmware operates differently; by actively driving the `PTH1`/`PTH2` pins via a 16-bit SPI DAC (DAC80504), the pulse width / power transfer is directly controlled (higher DAC voltage → larger allowed duty cycle → more power delivered).

There are 13 DAC steps (0–12), each corresponding to an increasing power transmission level. It is important to understand what the step actually sets: PTH is a **ceiling** on the LTC4125's internal pulse-width search, not a forced output level. Lowering the step changes delivered power only once the ceiling constrains the search below what the load is drawing. Consequently the control algorithm searches for the lowest ceiling the IPG still accepts — see **6.3 Adaptive Power Control** — and PGOOD typically stays high through several descending steps before anything changes.

| DAC voltage | Step | Role |
|-------------|------|------|
| 400 mV | 0 (minimum) | Floor of the search range |
| 500 mV | 1 | — |
| … | … | — |
| 1100 mV | 7 | Cold-start level, and where the closed loop begins |
| … | … | — |
| 1600 mV | 12 (maximum) | Maximum — cold-start escalation target |

**CTD pin:** LTC4125 `CTD` (pin 14) has a 36 pF capacitor to GND, which sets the delay between OPS cycles. MCU pin **P0.05 (`WPT_CTD_CTRL`)** drives a MOSFET gate; when that MOSFET is on, it shorts the capacitor and ties `CTD` directly to GND.

`WptManager::ConfigureGpios()` configures P0.05 as an output and writes it **HIGH** during initialization, then never changes it — so for the entire session the MOSFET is on and `CTD` is held at GND, bypassing the 36 pF timing capacitor.

> ⚠️ **Unresolved — needs a datasheet check and a scope on pin 14.** Three things point the same direction and have not been reconciled:
> - The GPIO configuration sits under a comment claiming it is "commented out because the CTD pin is not implemented in the prototype." It is not commented out; it is live code.
> - `Wpt_LTC4125::StopSearch()` and `ResumeSearch()` (`hal_wpt.cpp:59-69`) are empty stubs whose own comments state *"Write 1 to the CTD pin to stop the search"* and *"Write 0 to the CTD pin to resume."* By that convention, initialization writes the **stop-search** value, and the functions intended to control it do nothing. `StopWptScan()` therefore has no effect.
> - An earlier revision of this report asserted that grounding CTD stops the search after the first OPS cycle.
>
> Against that, power delivery demonstrably works and PGOOD does go high in testing. Either the polarity interpretation is wrong, or a single OPS cycle is sufficient given that the DAC drives PTH directly and overrides the search anyway. Worth settling before relying on `StopSearch()`/`ResumeSearch()` for anything.

### 2.2 DAC — DAC80504

16-bit quad-channel SPI DAC. Only CHANNEL_1 is used to drive `PTH1`/`PTH2`. `Dac80504::Init()` configures it with `ReferenceDivider::DIVIDER_OFF` and **`Gain::DOUBLE_GAIN`** (`hal_dac.cpp:35`), so with `REFERENCE_VOLTAGE = 2500 mV` the full-scale output is **5000 mV**, not 2500 mV. The `LDAC` pin is toggled after each SPI write to latch the new value.

```
DAC voltage = 400 mV + (step × 100 mV)    step ∈ [0, 12]
```

> **Note:** `hal_dac.h:65` defines `MAX_OUTPUT_VOLTAGE = 2500`, which is inconsistent with the configured 2× gain. That constant is unused anywhere in the codebase — do not treat it as the full-scale value.

### 2.3 Key GPIO / Analog Pins

Values below are for `BOARD == PCBA`. `hal_pinout.h:65-111` defines a materially different `DEV_KIT` pinout (e.g. `WPT_EN` on P0.28, `WPT_NTC` on P0.29 as a plain GPIO rather than AIN6) — check the build's `BOARD` setting before using these for bring-up.

| Signal | Pin | Direction | Notes |
|--------|-----|-----------|-------|
| `WPT_EN` | P1.15 | Output | LOW = LTC4125 enabled (active-low) |
| `WPT_STAT` | P0.7 | Input | LTC4125 load-detection status (see §11 — `GetStat()` is a stub) |
| `WPT_CTD_CTRL` | P0.5 | Output | HIGH = MOSFET on = LTC4125 `CTD` shorted to GND (see §2.1) |
| `WPT_NTC` | AIN6 | Analog | Local NTC thermistor on charger PCB (see §11 — conversion is broken) |
| `WPT_IMON` | AIN5 | Analog | Coil current monitor |
| `DAC_CS` | P0.11 | Output | DAC chip select (SPI) |
| `DAC_LDAC` | P0.12 | Output | DAC latch |
| `PMC_VCC_EN` | P0.20 | Output | Enables power management circuit |
| `SCLK` | P1.9 | Output | SPI clock |
| `MOSI` | P1.8 | Output | SPI MOSI |
| `MISO` | P0.8 | Input | SPI MISO |

### 2.4 LED Indicators

| LED state | Meaning |
|-----------|---------|
| Blue (scanning) | Scanning for IPG — BLE scan active |
| White (slow charge) | Coil powering up a switched-off IPG — waiting for BLE |
| Yellow (charging) | IPG found, WPT active, batteries charging |
| Green (charged) | All batteries full |
| Magenta (manual) | **Debug builds only** — `StateManual`, operator driving the coil over RTT (§4.7) |

---

## 3. Firmware Architecture

### 3.1 Layer Structure

```
┌─────────────────────────────────────────────┐
│              Application Layer              │
│  app_state_machine.cpp / app_system.cpp     │
│  state_wait / state_scan / state_charge /   │
│  state_slow_charge_and_scan /               │
│  state_initialization /                     │
│  state_manual (debug builds only)           │
├─────────────────────────────────────────────┤
│              Service Layer                  │
│  BLE: svc_ble_manager / svc_ble_subsystem   │
│  WPT: svc_wpt_manager / svc_wpt_subsystem   │
│  PMC: svc_pmc_subsystem                     │
│  Debug: svc_debug_console (debug builds)    │
├─────────────────────────────────────────────┤
│               Core Layer                    │
│  EDA framework: state machines, active      │
│  objects, ports, timers, queues             │
├─────────────────────────────────────────────┤
│               HAL Layer                     │
│  hal_ble / hal_wpt / hal_dac / hal_led /    │
│  hal_dfu / hal_adc                          │
└─────────────────────────────────────────────┘
```

### 3.2 Port / Event Model

Each subsystem exposes a **port** (a message queue). State machines communicate by calling `Port::SendEvent(event, optData)` rather than calling each other directly. This decouples layers — the application layer sends `WptPort::WPT_POWER_ON` and the WPT service handles it in its own context.

**System events** (`app_port.h`):

| Event | Meaning |
|-------|---------|
| `BLE_INITIALIZED` | BLE stack ready |
| `BLE_DEVICE_FOUND` | IPG advertisement received |
| `BLE_SCAN_TIMEOUT` | No advertisement within scan window |
| `WPT_SCAN_TIMEOUT` | WPT-level scan timed out |
| `BATTERY_CHARGED` | All batteries full |
| `BUTTON_PRESSED` | User pressed the power button |
| `BUTTON_DFU_PRESSED` | DFU mode button combo |
| `TURN_OFF` | Formerly the thermal shutdown path; thermal faults no longer reach the application layer at all (§6.5). In a **production** build nothing sends it and the handler at `state_charge.cpp:60` is unreachable. In a **debug** build the RTT console's `n` command sends it to leave `StateManual` (`state_manual.cpp:67`). |
| `MANUAL_TAKEOVER` | **Debug builds only** (0x12). Sent by the RTT console's `m` command; each operational state transitions to `StateManual` on it (§4.7). Safe despite `MAX_EVENT_ENUM_LENGTH = 20`: `SystemPort::ExecuteEvent` dispatches straight to the state machine and never indexes `mEventCallback`. |

**WPT service events** (`svc_wpt_port.h`):

| Event | Value | Meaning |
|-------|-------|---------|
| `WPT_POWER_ON` | 0x03 | Enable LTC4125, begin OPS |
| `WPT_POWER_OFF` | 0x07 | Disable LTC4125 |
| `WPT_FAULT_CONDITION` | 0x08 | Fault detected |
| `WPT_BATTERY_CHARGING` | 0x09 | BLE confirms charging in progress |
| `WPT_BATTERY_CHARGED` | 0x0B | BLE confirms all batteries full |
| `WPT_SLOW_CHARGE` | 0x0C | Begin the open-loop cold-start attempt (step 7, escalating to max) |
| `WPT_SCAN_TIMEOUT` | 0x0D | Power search timed out |
| `WPT_ADJUST_POWER` | 0x0E | Set specific DAC step. Handled in `StateCharging` and `StateSlowCharge`; **dropped** in `StateIdle` (§6.1) |
| `WPT_FAULT_PAUSE` | 0x0F | Suspend coil output; `optDataAddress` carries the `PauseReason_e` (`PAUSE_THERMAL`, `PAUSE_OVP`, or in debug builds `PAUSE_MANUAL`) |
| `WPT_FAULT_RESUME` | 0x10 | Clear one pause reason; `optDataAddress` carries the `PauseReason_e` |
| `WPT_MANUAL_IDLE` | 0x11 | **Debug builds only.** Sent by `StateManual::Entry()`; every WPT state stops the coil and settles in `StateIdle` with fault monitoring only (§4.7) |

`WPT_FAULT_PAUSE`/`WPT_FAULT_RESUME` replaced the earlier `WPT_THERMAL_PAUSE`/`WPT_THERMAL_RESUME` pair at the same event values, generalized to carry a reason so thermal and OVP can share one pause mechanism (see §6.2). The debug console reuses the same pair with `PAUSE_MANUAL` for its coil stop/start. Manual mode adds exactly one WPT event, `WPT_MANUAL_IDLE` (0x11, debug builds only) — kept to one because callbacks registered on a port are stored in a 20-entry array indexed by event ID (`eda_port.h:13`).

---

## 4. Application State Machine

### 4.1 States and Transitions

```
                     power on
                         │
                         ▼
                ┌─────────────────┐
                │  StateInit      │ ← BLE + WPT + PMC subsystems initialized
                └────────┬────────┘
                         │ BLE_INITIALIZED
                         ▼
         ┌──────────────────────────────┐
         │           StateWait          │ ← LEDs off, idle
         └──────────────┬───────────────┘
                        │ BUTTON_PRESSED
                        ▼
         ┌──────────────────────────────┐
         │          StateScan           │ ← Blue LED, BLE scanning (10 s)
         └────┬─────────────────────────┘
              │                         │
    BLE_DEVICE_FOUND            BLE_SCAN_TIMEOUT
              │                         │
              ▼                         ▼
   ┌──────────────────┐    ┌────────────────────────────┐
   │   StateCharge    │    │   StateSlowChargeAndScan   │ ← White LED
   │  (Yellow LED)    │    │   Cold start: step 7, then │
   └──────┬───────────┘    │   max after 5 s            │
          │                │   BLE scanning (10 s)      │
          │                └──────────┬─────────────────┘
          │                          │ BLE_DEVICE_FOUND
          │                          │
          │    ◄─────────────────────┘
          │
          │ BATTERY_CHARGED → StateWait (green LED)
          │ BUTTON_PRESSED  → StateWait
          │ TURN_OFF        → StateWait   (unreachable in production builds — see §3.2)
          │ WPT_SCAN_TIMEOUT → StateWait  (unreachable — see §11, GetStat() stub)
```

**Debug builds add `StateManual` (§4.7).** `StateWait`, `StateScan`, `StateSlowChargeAndScan` and `StateCharge` each move to it on `MANUAL_TAKEOVER`; only `TURN_OFF` (the console's `n`) returns it to `StateWait` — Button 1 is ignored there. It is not drawn above because it does not exist in a production build.

### 4.2 StateInitialization

Entry sends `INITIALIZE` to the BLE, WPT, and PMC subsystem ports. When `BLE_INITIALIZED` is received, transitions to `StateWait`. This is the only state where subsystem initialization occurs.

### 4.3 StateWait

Idle state. No LEDs, no scanning, no WPT. Transitions to `StateScan` on `BUTTON_PRESSED`, and in debug builds to `StateManual` on `MANUAL_TAKEOVER`. "No WPT" does not imply VCC_EN is low: arriving from `StateCharge` leaves the rail up, because `StateCharge::Exit()` never sends `PMC_POWER_OFF` (§4.6).

### 4.4 StateScan

```
Entry:
  BlePort::START_SCANNING
  hal::Leds::LedScanOn(true)       ← blue LED

DispatchEvent:
  BLE_DEVICE_FOUND:
    mWptManager.StartIpgTemperaturePgoodMonitoringTimer()
    → ChangeState(StateCharge)

  BLE_SCAN_TIMEOUT:
    mWptManager.StopIpgTemperaturePgoodMonitoringTimer()
    → ChangeState(StateSlowChargeAndScan)

  BUTTON_DFU_PRESSED:
    hal::Dfu::start_dfu_mode()

  MANUAL_TAKEOVER:                    (debug builds only)
    → ChangeState(StateManual)

Exit:
  WptPort::WPT_POWER_ON            ← transitions WPT SM StateIdle → StateCharging
  hal::Leds::LedScanOn(false)
```

When no IPG is found within the 10-second scan window, the charger assumes the IPG may be powered off. It enters `StateSlowChargeAndScan`, which drives the coil hard enough to boot a drained IPG from wireless power, then scans for another 10 seconds to catch the BLE advertisement after initialization.

Note the ordering: `ChangeState` runs `StateScan::Exit()` (which sends `WPT_POWER_ON`, taking the WPT SM from `StateIdle` to `StateCharging`) *before* `StateSlowChargeAndScan::Entry()` sends `WPT_SLOW_CHARGE`. So `WPT_SLOW_CHARGE` is always handled by `StateCharging`, never by the WPT SM's own `StateSlowCharge` — see §11.

### 4.5 StateSlowChargeAndScan

```
Entry:
  BleManager::SetScanTimeout(10000)     ← 10 s window for IPG boot time
  BlePort::START_SCANNING
  WptPort::WPT_SLOW_CHARGE              ← cold start: step 7, escalating to max at 5 s
  PmcPort::PMC_POWER_ON                 ← VCC_EN high (5V rail for LTC4125)
  hal::Leds::LedChargingSlow(true)      ← white LED

DispatchEvent:
  BLE_DEVICE_FOUND:
    mWptManager.StartIpgTemperaturePgoodMonitoringTimer()
    → ChangeState(StateCharge)          ← WPT and VCC stay enabled

  BLE_SCAN_TIMEOUT:
    BlePort::STOP_SCANNING
    mWptManager.StopIpgTemperaturePgoodMonitoringTimer()
    WptPort::WPT_POWER_OFF
    PmcPort::PMC_POWER_OFF              ← VCC_EN low
    → ChangeState(StateWait)

  WPT_SCAN_TIMEOUT:
    WptPort::WPT_POWER_OFF
    PmcPort::PMC_POWER_OFF              ← VCC_EN low
    → ChangeState(StateWait)

  BUTTON_DFU_PRESSED:
    hal::Dfu::start_dfu_mode()

  MANUAL_TAKEOVER:                    (debug builds only)
    → ChangeState(StateManual)

Exit:
  BleManager::SetScanTimeout(10000)     ← restore default (same value since the change below)
  hal::Leds::LedChargingSlow(false)
```

**Cold-start power profile:** `WPT_SLOW_CHARGE` no longer clamps the DAC to minimum. Minimum power (step 0, 400 mV) was never enough to bring a fully drained IPG's VRECT up to boot, which is the entire purpose of this state. Instead `WptManager::StartColdStartEscalation()` drives step 7 immediately and starts a 5 s one-shot timer; if no advertisement has been parsed by then, it escalates to maximum power for the remainder of the window.

This window is open-loop by necessity — with no BLE there is no PGOOD and no OVP telemetry, so nothing can be steered by. The escalation callback self-guards on `BleManager::GetAdvertisementCount() == 0`, so if an advertisement arrives while the timer is pending it fires harmlessly and the closed loop keeps ownership of the power level. That removes any need to cancel the timer on the BLE-found path.

The scan window was shortened from 30 s to 10 s at the same time. If wake-up of a drained IPG starts failing after this change, this is the first constant to lengthen — max power shortens the IPG's boot time but not the time it needs to start advertising afterwards.

**Key design decisions on BLE_DEVICE_FOUND:** When the IPG is detected, WPT and VCC are left enabled intentionally. `StateCharge::Entry()` sends `WPT_POWER_ON` to ensure the WPT service is in `StateCharging` regardless of which path led here, and `PMC_POWER_ON` is a no-op in `PmcStateEnable` (PMC was already enabled in this state's `Entry()`). This avoids any disable/enable round-trip that would briefly cut power to the still-booting IPG.

**Why PMC_POWER_ON is needed in this state:** `PIN_PMC_VCC_EN` (P0.20) controls the 5V supply rail that powers the LTC4125. Without it, the LTC4125 has no supply and cannot transmit power even if `WPT_EN` is asserted. Since `StateSlowChargeAndScan` is specifically for charging a fully-discharged IPG (no BLE yet), VCC must be enabled here, not deferred to `StateCharge`.

### 4.6 StateCharge

```
Entry:
  PmcPort::PMC_POWER_ON
  hal::Leds::LedCharging(true)          ← yellow LED
  WptPort::WPT_POWER_ON                 ← ensures WPT active from any entry path
  mWptManager.StartIpgTemperaturePgoodMonitoringTimer()

DispatchEvent:
  BATTERY_CHARGED:
    hal::Leds::LedCharged(true)         ← green LED
    → ChangeState(StateWait)

  BUTTON_PRESSED:
    → ChangeState(StateWait)

  TURN_OFF:
    → ChangeState(StateWait)            ← unreachable in production builds (§3.2)

  BLE_SCAN_TIMEOUT:
    BlePort::START_SCANNING             ← restart scan, do NOT exit charge state

  WPT_SCAN_TIMEOUT:
    → ChangeState(StateWait)            ← UNREACHABLE: see §11, GetStat() is a stub

  BUTTON_DFU_PRESSED:
    hal::Dfu::start_dfu_mode()

  MANUAL_TAKEOVER:                    (debug builds only)
    → ChangeState(StateManual)

Exit:
  BlePort::STOP_SCANNING
  WptPort::WPT_POWER_OFF
  hal::Leds::LedCharging(false)
```

**BLE_SCAN_TIMEOUT handling in StateCharge:** Once the IPG detects wireless power, it typically reduces its BLE advertising rate to conserve power. The 10-second scan timeout would otherwise kick the charger out of `StateCharge` and disable WPT. Instead, when a scan timeout fires while in `StateCharge`, BLE scanning is restarted immediately. The charger remains in `StateCharge` and the monitoring loops continue uninterrupted. WPT is never disabled on a scan timeout in this state.

That reduced advertising rate has a second consequence the control loop has to handle directly: `GetAdvertisementData()` returns the last parsed advertisement indefinitely, so a PGOOD sample can be seconds old and predate the power step being evaluated. See §5.2 and §6.3 for the freshness guard that addresses it.

**Thermal and OVP faults do not exit this state.** Both are handled entirely inside the WPT service layer by pausing coil output while leaving the state machines in place (§6.2). The application layer stays in `StateCharge`, the yellow LED stays on, and recovery needs no button press.

**`Exit()` does not drop VCC_EN.** No `PMC_POWER_OFF` is sent, so leaving `StateCharge` on `BATTERY_CHARGED` or `BUTTON_PRESSED` parks PMC in `PmcStateEnable`: the LTC4125 stays powered, though disabled (`WPT_EN` high), in `StateWait`. Harmless for coil output, but worth knowing when probing VCC_EN — and the reason `StateManual::Exit()` sends `PMC_POWER_OFF` explicitly (§4.7).

### 4.7 StateManual (debug builds only)

Exists only when `WPT_MANUAL_DEBUG_MODE` is 1 (§10); a production build contains none of it. It lets an operator drive the coil by hand over RTT while normal logging continues. Background and rationale are in §8.4.

```
Entry:
  DebugConsole::OnManualEntered()         ← the state owns the console's manual flag
  PmcPort::PMC_POWER_ON                   ← VCC_EN high
  BlePort::START_SCANNING                 ← IPG telemetry for the warn-only monitors
  WptPort::WPT_MANUAL_IDLE                ← queued behind the previous state's WPT events:
                                            coil off, timers stopped, escalation cancelled,
                                            level → step 7, WPT SM → StateIdle, then
                                            fault monitoring only
  magenta LED

DispatchEvent:
  TURN_OFF:            → ChangeState(StateWait)   ← console 'n' — the only way out
  BUTTON_PRESSED:      ignored, logged            ← Button 1 disabled in manual mode
  BLE_SCAN_TIMEOUT:    BlePort::START_SCANNING    ← re-arm, no state change
  BATTERY_CHARGED:     BlePort::START_SCANNING    ← undo ProcessNewBleData()'s STOP_SCANNING
  BUTTON_DFU_PRESSED:  hal::Dfu::start_dfu_mode()
  (WPT_SCAN_TIMEOUT and BLE_DEVICE_FOUND deliberately absent)

Exit:
  DebugConsole::OnManualExited()
  mWptManager.StopIpgTemperaturePgoodMonitoringTimer()
  WptPort::WPT_POWER_OFF
  BlePort::STOP_SCANNING
  PmcPort::PMC_POWER_OFF                  ← required; nothing else drops the rail
  LEDs off
```

It is entered from `StateWait`, `StateScan`, `StateSlowChargeAndScan` or `StateCharge` on `MANUAL_TAKEOVER`, which the console sends on `m`. Behaviour that must not happen in manual mode is expressed as a missing case rather than a runtime check, which is why a scan timeout with no IPG present cannot end the session the way it does in `StateSlowChargeAndScan` (§8.4). Whatever the previous state was doing — scanning, a cold start, an automatic charge — is stopped on entry, and the board idles with the coil off until `s`. Button 2, which resets the MCU from its ISR, remains the physical escape hatch.

Console commands (`svc_debug_console.cpp`, polled every 100 ms on the SEGGER RTT down-channel):

| Key | Action |
|---|---|
| `m` / `n` | enter / leave manual mode |
| `s` | coil start: `PMC_POWER_ON`, `WPT_POWER_ON`, `WPT_FAULT_RESUME(PAUSE_MANUAL)`, `WPT_ADJUST_POWER` — in that order, because WPT `StateIdle` drops the last two (§6.1) |
| `x` | coil stop: `WPT_FAULT_PAUSE(PAUSE_MANUAL)`. VCC_EN stays up, so only `WPT_ENn` moves |
| `+` / `-`, `0`–`9`, `a`–`c` | step, or set, PTH 0–12 via `SetPowerLevelManual()` |
| `?` / `h` | status (mode, coil, pause mask, app state, level and mV, bounds, IPG bits, dead-man) / help |

Thermal and OVP are warn-only (§6.5, §6.6), the raw IPG fault bits are logged every fault tick, and a dead-man timeout pauses the coil after 10 minutes with no keystroke.

**Why entry goes through a WPT event (fixed 2026-09-10).** An earlier revision did the WPT setup directly in `Entry()` and failed for three of the four entry states, all for one reason. `ChangeState()` runs the previous state's `Exit()` and `StateManual::Entry()` back-to-back on the SYSTEM task, which runs at priority `app` (4) against the WPT task's `svc_1` (1), so whatever the previous `Exit()` queued to the WPT port is processed *after* `Entry()`:

| Entered from | Queued by the previous `Exit()` | Effect of direct setup in `Entry()` |
|---|---|---|
| `StateWait` | nothing | worked |
| `StateScan` | `WPT_POWER_ON` | coil came on once VCC_EN rose |
| `StateSlowChargeAndScan` | nothing — coil already on | coil stayed on; a pending cold-start escalation could take it to step 12 |
| `StateCharge` | `WPT_POWER_OFF` | its `DisableWpt()` stopped the fault timer `Entry()` had just started |

`WPT_MANUAL_IDLE` is queued behind those events on the same port, so FIFO order makes it the last word; its handler is `WptManager::EnterManualIdle()`. A cold-start escalation expiring at the same moment cannot leave the level at step 12: `DisableWpt()` posts the timer's stop before `ResetPowerControl()`, and the FreeRTOS timer task (priority 2) preempts the WPT task (1) on that post, so the escalation runs before the reset, not after. The one visible residue is entry from `StateScan`, whose queued `WPT_POWER_ON` still drives `WPT_ENn` low for well under a millisecond before `WPT_MANUAL_IDLE` is processed — normally with VCC_EN still off, since `StateScan` never raises it.

The same revision let Button 1 leave manual mode without clearing the console's flag, which demoted thermal and OVP to warn-only in the next automatic session. Button 1 is now ignored in `StateManual`, and the flag is owned by the state itself (`OnManualEntered()`/`OnManualExited()`), so the two cannot disagree.

---

## 5. BLE Service Layer

### 5.1 BLE State Machine

Two states:

- **StateIdle** — On `INITIALIZE`: calls `BleManager::Init()`, sends `BLE_INITIALIZED`. On `START_SCANNING` → `StateScanning`.
- **StateScanning** — Entry: `BleManager::StartScanning()`. On `STOP_SCANNING` or `SCAN_TIMEOUT` → `StateIdle`. Exit: `BleManager::StopScanning()`.

### 5.2 BLE Manager

`BleManager` owns scanning and advertisement parsing. Its scan timeout is a software watchdog timer (`eda::Timer`) that fires if no valid advertisement arrives within `mScanTimeoutMs`. On each valid IPG advertisement received, the timer is restarted (from ISR context) — so the timeout only fires if the IPG goes silent.

**Scan timeout values:**

| Constant | Value | Used in |
|----------|-------|---------|
| `SCAN_TIMEOUT_MS` | 10,000 ms | `StateScan`, `StateCharge` |
| `SCAN_TIMEOUT_SLOW_CHARGE_MS` | 10,000 ms | `StateSlowChargeAndScan` |

`SetScanTimeout(ms)` must be called before `StartScanning()` (or between advertisements) to take effect. `StateSlowChargeAndScan::Entry()` calls it with `SCAN_TIMEOUT_SLOW_CHARGE_MS`; `StateSlowChargeAndScan::Exit()` restores `SCAN_TIMEOUT_MS`. The two are currently equal, but the calls are kept distinct so the cold-start window can be retuned independently.

**Advertisement counter.** `BleManager` maintains `mAdvertisementCounter`, incremented in `ParseManufacturerSpecificData()` immediately after the payload fields are written and before `DEVICE_FOUND` is sent, exposed via `GetAdvertisementCount()`. Ordering matters: any consumer that observes a new count is guaranteed to see the data that arrived with it.

It exists because `GetAdvertisementData()` alone gives no way to tell how old its contents are, and two separate failure modes follow from that:

- **Validity.** A count of zero means no advertisement has *ever* been parsed, so `mAdvertisementData` is still all zeros. Since the IPG's fault bits are active-low, all-zero reads as *every fault asserted*, and the all-zero thermistor fields make the resistance calculation degenerate into a 50 °C reading. Both the OVP and temperature monitors bail out while the count is zero — without that, the charger would fault-pause on every startup.
- **Freshness.** An unchanged count between two control decisions means no new telemetry arrived, so the previous decision has not been observed yet. The power control loop skips such cycles rather than acting on a stale PGOOD (§6.3).

### 5.3 Advertisement Parsing

The charger scans for manufacturer-specific data with company ID `0xF0F0` (production IPG firmware). When a matching advertisement is found, `ParseManufacturerSpecificData()` extracts the payload and stores it in `mAdvertisementData`. The timer is then restarted and `BlePort::DEVICE_FOUND` is sent to the application layer.

**IPG MSD layout** (bytes relative to start of MSD field, after length and type bytes):

```
index+2, index+3  Company ID (little-endian): 0xF0F0
index+4           msd[0]  DVDD voltage        × 100 mV
index+5           msd[1]  Battery A voltage   × 100 mV
index+6           msd[2]  Battery B voltage   × 100 mV
index+7           msd[3]  Impedance A         × 10 mV
index+8           msd[4]  Impedance B         × 10 mV
index+9           msd[5]  ThermRef voltage    × 10 mV
index+10          msd[6]  ThermOut voltage    × 10 mV
index+11          msd[7]  ThermOfst voltage   × 10 mV
index+12          msd[8]  GPIO bits 0–7
index+13          msd[9]  GPIO bits 8–13
…
index+27          msd[23] Hardware version
```

**GPIO byte (msd[8]) bit assignments:**

| Bit | Signal | Active level | Meaning when set (1) |
|-----|--------|-------------|----------------------|
| 0 | `VRECT_DETn` | Active-low | No coil detected |
| 1 | `VRECT_OVPn` | Active-low | Rectifier voltage OK |
| 2 | `VCHG_PGOOD` | Active-high | Boost converter output good |
| 3 | `CHG1_STATUS` | — | Battery A idle / charge complete |
| 4 | `CHG1_OVP_ERRn` | Active-low | Battery A OVP OK |
| 5 | `CHG2_STATUS` | — | Battery B idle / charge complete |
| 6 | `CHG2_OVP_ERRn` | Active-low | Battery B OVP OK |
| 7 | `ENG2_SDNn` | Active-low | ENG2 enabled |

> ⚠️ **The parser stores these as raw bits, not normalized booleans.** `GET_VRECT_OVP`, `GET_CHG1_OVP_ERR`, and `GET_CHG2_OVP_ERR` hold the bit exactly as received (`svc_ble_manager.cpp:182,185,187`), so for all three an **asserted fault is 0, not 1**. A natural-looking `if (params.GET_VRECT_OVP)` is inverted and will trip on every healthy advertisement. Only `GET_VCHG_RAIL_SUPPLY_CIRCUIT_POWER_GOOD` (bit 2) is active-high and reads the way its name suggests.

**Thermistor bytes** are in units of 10 mV; the parser multiplies by 10 to produce mV for `CalculateTemperatureFromBle()`. Note the IPG clamps each to 0xFF before transmitting (`app_mode_ble_active.c:137-139`), giving a ceiling of 2550 mV, and the 10 mV quantization is ±~0.6 °C at the charger — appreciable against the 2 °C thermal hysteresis band.

**Battery voltage** is packed as:
```
BATTERY_VOLTAGE_MEASURED = (BattA_mV) | (BattB_mV << 16)
where BattA_mV = msd[1] * 100,  BattB_mV = msd[2] * 100
```

---

## 6. WPT Service Layer

### 6.1 WPT State Machine

Four states:

```
StateIdle
  │ WPT_POWER_ON → StateCharging
  │ WPT_SLOW_CHARGE → StateSlowCharge
  │ INITIALIZE: mWptManager.Init()
  │ WPT_MANUAL_IDLE: mWptManager.EnterManualIdle()          (debug builds)
  │ everything else is silently dropped — including WPT_ADJUST_POWER,
  │ WPT_FAULT_PAUSE and WPT_FAULT_RESUME

StateSlowCharge
  │ Entry: mWptManager.EnableWpt()
  │ WPT_POWER_ON / WPT_LOAD_DETECTED → StateCharging
  │ WPT_POWER_OFF → StateIdle (calls mWptManager.DisableWpt())
  │ WPT_SCAN_TIMEOUT → StateIdle
  │ WPT_ADJUST_POWER: mWptManager.AdjustWptPowerTransfer(step)   ← added 2026-09-09
  │ WPT_FAULT_PAUSE:  mWptManager.PauseWpt(reason)   — no state change
  │ WPT_FAULT_RESUME: mWptManager.ResumeWpt(reason)  — no state change
  │ WPT_MANUAL_IDLE: EnterManualIdle() → StateIdle          (debug builds)

StateCharging
  │ Entry: mWptManager.EnableWpt()
  │ WPT_POWER_ON: no-op (already active)
  │ WPT_POWER_OFF → StateIdle (calls mWptManager.DisableWpt())
  │ WPT_SLOW_CHARGE: mWptManager.StartColdStartEscalation()
  │ WPT_ADJUST_POWER: mWptManager.AdjustWptPowerTransfer(step)
  │ WPT_BATTERY_CHARGED: no-op (handled by app layer — see §6.4)
  │ WPT_SCAN_TIMEOUT: → WPT_POWER_OFF
  │ WPT_FAULT_CONDITION: → WPT_POWER_OFF
  │ WPT_FAULT_PAUSE:  mWptManager.PauseWpt(reason)   — no state change
  │ WPT_FAULT_RESUME: mWptManager.ResumeWpt(reason)  — no state change
  │ WPT_MANUAL_IDLE: EnterManualIdle() → StateIdle          (debug builds)

StateTest (stub — Entry and DispatchEvent log only, no behavior)
```

**Why the fault cases change no state:** a thermal or OVP pause must stop coil output without tearing down monitoring, because the recovery condition can only be observed by continuing to sample. Staying in `StateCharging` also keeps the application layer in `StateCharge`, so recovery is automatic rather than requiring a button press. Both WPT states carry the cases because the monitoring timers also run while scanning and slow-charging, so a fault can arrive in `StateSlowCharge` and not only in `StateCharging`.

**`StateIdle` drops more than it appears to.** Its `default:` case (`svc_wpt_state_idle.cpp:72`) silently discards every event except `INITIALIZE`, `WPT_POWER_ON`, `WPT_SLOW_CHARGE` and, in debug builds, `WPT_MANUAL_IDLE`. A power adjustment, pause or resume that arrives while the WPT SM is idle is lost, not deferred. Any sequence that must land in `StateCharging` therefore has to send `WPT_POWER_ON` first — which is why the ordering of the debug console's coil-start command is load-bearing (§4.7).

**`StateSlowCharge` used to drop `WPT_ADJUST_POWER` as well.** Every power step arriving there was discarded and the DAC kept its previous value. The state is currently unreachable (§11), so this never bit in practice, but it would have the moment it became reachable. Fixed on 2026-09-09, independently of the debug work.

### 6.2 WPT Manager

`WptManager` is the singleton that owns the LTC4125 and DAC directly.

**`EnableWpt()`:**
1. Sets `WPT_EN` LOW (active-low: enables LTC4125) — **only if the pause mask is clear**
2. Starts the 5-second status timeout timer
3. Starts the status monitoring timer (500 ms periodic)

The mask check in step 1 matters because `Entry()` re-runs on the `StateSlowCharge → StateCharging` transition. Without it, a `WPT_POWER_ON` arriving during an active thermal or OVP pause would turn the coil back on underneath that pause. A fault outranks a state entry.

**`DisableWpt()`:**
1. Sets `WPT_EN` HIGH (active-low: disables LTC4125)
2. Stops the status monitoring timer
3. Stops both monitoring timers (fault and power control)
4. Stops the cold-start escalation timer
5. Calls `ResetPowerControl()` — clears the pause mask, both pause tick counters, the level, the floor/ceiling bounds, and the loop-initialized flag

Step 5 is what makes a charge session start clean. An earlier revision left the thermal flag as a static that survived across sessions, so a session that ended while paused would leave it set and the next session's first cool reading would fire a spurious resume.

**`PauseWpt(reason)` / `ResumeWpt(reason)`:**
Thermal and OVP faults share one pause mechanism, arbitrated by a bitmask:

```cpp
enum PauseReason_e : uint8_t {
    PAUSE_THERMAL = 1 << 0,
    PAUSE_OVP     = 1 << 1,
    PAUSE_MANUAL  = 1 << 2,   // debug console only — see §4.7
};
static uint8_t m_pause_reasons;   // coil enabled iff == 0
```

`PauseWpt()` disables the coil only on the first reason set; `ResumeWpt()` re-enables it only once the mask returns to zero. With two independent booleans instead, overlapping faults would resume each other — whichever cleared first would restore power while the other was still asserted. Neither function touches the monitoring timers.

`PAUSE_MANUAL` is set and cleared only by the RTT debug console. Putting it in the same mask means a manual stop cannot be undone by an automatic fault resume, and the "coil enabled iff mask == 0" rule keeps holding without a special case.

**Coil state tracking — `m_coil_enabled` / `IsCoilEnabled()`.** The mask alone cannot say whether the coil is being driven: in WPT `StateIdle` the mask is zero *and* the coil is off. `m_coil_enabled` records the actual drive state. It is set wherever `WptHalInstance.Enable()` really runs — `EnableWpt()` on its unmasked branch, and `ResumeWpt()` when the mask reaches zero — and cleared in `DisableWpt()` and on the first `PauseWpt()`.

**Monitoring timer entry points.** `StartIpgTemperaturePgoodMonitoringTimer()` starts both the fault timer and the power-control timer (§6.3). `StartFaultMonitoringOnly()` starts only the fault timer; manual mode uses it so thresholds are still evaluated while the titration loop never runs. In debug builds `EnterManualIdle()` combines what manual mode needs on entry — `DisableWpt()` then `StartFaultMonitoringOnly()` — and runs on the WPT task via `WPT_MANUAL_IDLE` (§4.7). There is a single stop, `StopIpgTemperaturePgoodMonitoringTimer()`, which stops both.

**Accessors for the debug console.** `SetPowerLevelManual()` routes through `SetPowerLevel()`, so `m_level` stays the single source of truth. Alongside it: `GetPowerLevel()`, `GetMaxPowerLevel()`, `GetPauseReasons()`, `GetOvpCeiling()`, `GetPgoodFloor()`, `IsFloorFound()`, and `IsCoilEnabled()` — the last read only by the console's status dump. `RearmPowerSearch()` clears `m_floor_found` while keeping the observed bounds; it currently has **no callers**. It was written for an exit-to-automatic handover that was replaced by a full shutdown on exit, and is kept in case that handover is wanted.

**`AdjustWptPowerTransfer(step)`:**
Calls `SetPulseWidthThresholdStep(step)`, which writes:
```
voltage_mV = 400 + (step × 100)
dac_code   = voltage_mV × 65536 / (gain × VREF)
           = voltage_mV × 65536 / (2 × 2500)     ← gain is 2, see §2.2
```
to DAC CHANNEL_1 and latches via `LDAC`. Step 12 → 1600 mV → dac_code 20971. Steps above the maximum are clamped to `VoltageMaxPulseWidthThreshold_mV` by the HAL (`hal_wpt.cpp:106-109`), which is what lets the cold-start path request "maximum" with a deliberately out-of-range constant.

`WptManager::SetPowerLevel()` normalizes before recording, so `m_level` always names a real step and never the out-of-range max request.

### 6.3 Adaptive Power Control

The goal is unchanged from earlier revisions: transmit at the minimum power needed to keep the IPG's boost converter in regulation, rather than running at full power for the whole session. The mechanism is entirely different.

#### Two timers, two rates

The single 2-second timer that formerly drove both temperature and PGOOD has been split, because the two loops have opposite requirements:

| Timer | Period | Drives |
|-------|--------|--------|
| `mFaultTimer` | 2 s | `IpgOvpMonitoring()`, then `IpgTemperatureMonitoring()` |
| `mPowerCtrlTimer` | 10 s | `PowerControlMonitoring()` |

Faults need the fast rate: the IPG holds itself in its own PAUSED state for only 5 s after a VRECT OVP event before re-enabling, so the charger has to see and react inside that window. Power steps need the slow rate: a PTH change must settle *and* propagate back through an IPG BLE advertisement before it can be evaluated, and at 2 s the loop was frequently deciding on telemetry that predated its own last change.

Every automatic path starts and stops them together through `StartIpgTemperaturePgoodMonitoringTimer()` / `StopIpgTemperaturePgoodMonitoringTimer()`. They can also be separated: `StartFaultMonitoringOnly()` starts the fault timer alone, and debug-build manual mode (§4.7) uses it. That is what makes "manual mode never titrates" structural rather than a runtime check — the power-control timer is simply never started. The single stop function still stops both.

#### The control law

`PowerControlMonitoring()` evaluates four rules:

| Observation | Meaning | Action |
|-------------|---------|--------|
| OVP asserted | Too much power | Down — handled by the fault path, §6.6 |
| PGOOD = 0, no OVP | Too little power | Up one step |
| PGOOD = 1, floor not yet found | Possibly overpowered | Down one step |
| PGOOD = 1, floor found | At the minimum viable level | Hold |

The loop starts at `COLD_START_LEVEL` (step 7) on the first cycle with real telemetry, wherever the open-loop cold-start attempt happened to leave the level. Because the error is correctly signed in every case, there is no reset-and-restart path, and every correction is a single 100 mV step.

Three guards run before any rule is evaluated:

```cpp
if (m_pause_reasons != 0)          return;   // fault path owns the level
if (m_blank_cycles) { m_blank_cycles--; return; }
if (adv_count == m_last_adv_count) return;   // stale telemetry, or cold start
```

The first is what makes PGOOD interpretable at all. Because the control loop bails out whenever any fault is asserted, every PGOOD = 0 it actually observes has no fault behind it — so it unambiguously means "too little power," and stepping up is correct. Faults only ever step down, and only from the 2 s path. See §6.6 for why this separation is necessary rather than merely tidy.

The third guard also cleanly gates the loop off during cold start, since the advertisement count stays at zero until the IPG answers.

#### Floor, ceiling, and the empty window

The loop remembers both bounds it discovers:

- `m_pgood_floor` — the highest level observed to be insufficient for PGOOD
- `m_ovp_ceiling` — the lowest level observed to trip an IPG OVP fault

The ceiling is not optional bookkeeping. Without it, the "PGOOD is low, add power" rule walks straight back into the level that just faulted the IPG, and the two controllers oscillate indefinitely.

At very close coupling the ceiling can sit *at or below* the floor — no power level satisfies the IPG without faulting it. `IsPowerWindowEmpty()` detects this (`m_pgood_floor + 1 >= m_ovp_ceiling`), and the loop then clamps to `m_ovp_ceiling - 1`, sets `m_floor_found`, stops searching, and logs at ERROR. Undervoltage charging is slow but stable; ping-ponging between a faulting level and an insufficient one delivers nothing at all.

#### Debounce and ratcheting

`PGOOD_LOW_CONFIRM = 2` requires two consecutive low readings before stepping up, rejecting single-sample dropouts at the cost of one extra cycle on a real insufficiency.

Once the floor is found the level only ratchets **up**, so the loop does not hunt for the rest of the session. The one event that re-arms the downward search is a thermal pause — a thermal trip is direct evidence that the settled level delivers more power than this coupling needs, which is exactly when re-hunting is worth the effort.

#### Timing expectations

Descending from step 7 takes up to 7 steps at 10 s each, plus 20 s to confirm the insufficiency at the bottom — roughly 1–2 minutes before the loop reaches `HOLD` in the typical case. A short bench test may never reach steady state; that is not a malfunction.

**Key constants:**

```cpp
COLD_START_LEVEL          = 7    // of 0..12 — where the closed loop begins
PGOOD_LOW_CONFIRM         = 2    // consecutive PGOOD=0 cycles before stepping up
BLANK_CYCLES_AFTER_FAULT  = 1    // control cycles skipped after a fault resume
MIN_POWER_LEVEL           = 0
MAX_VOLTAGE_STEP          = 12   // maximum DAC step (from the HAL)
```

### 6.4 Battery Charged Handling — Race Condition Avoidance

When the IPG reports all batteries full (`CHG1_STATUS = 1` AND `CHG2_STATUS = 1` AND `VCHG_PGOOD = 1`), the application layer sends `BATTERY_CHARGED` to the system port. `StateCharge::Exit()` then sends `WPT_POWER_OFF` through the normal port path.

`StateCharging::DispatchEvent` receives `WPT_BATTERY_CHARGED` but deliberately does **not** send `WPT_POWER_OFF` directly. Doing so would race with the `SlowChargeAndScan → Charge` transition: multiple `DEVICE_FOUND` events can be queued while `STOP_SCANNING` is still pending, and a second `WPT_BATTERY_CHARGED` would disable WPT after `StateCharge::Entry()` has already re-enabled it via `WPT_POWER_ON`. Routing all shutdown through the application layer's `StateCharge::Exit()` avoids this.

### 6.5 IPG Temperature Monitoring

This loop acts on the **IPG's** temperature, broadcast over BLE — not the charger's own coil NTC, which has no control logic at all (§11). `IpgTemperatureMonitoring()` runs on the 2-second fault timer. It reads `GET_THERM_REF`, `GET_THERM_OUT`, and `GET_THERM_OFST` from the BLE advertisement and calculates IPG temperature using the same 104AP-2 NTC lookup table used by the IPG firmware:

| Temperature | Resistance |
|-------------|-----------|
| 20°C | 126,400 Ω |
| 25°C | 100,000 Ω |
| 30°C | 79,590 Ω |
| 40°C | 51,320 Ω |
| 50°C | 33,790 Ω |

**Thermal actions:**

| Condition | Action |
|-----------|--------|
| temp ≥ 41 °C (`IPG_TEMP_THRESHOLD_PAUSE`), not already paused | `WPT_FAULT_PAUSE(PAUSE_THERMAL)` — coil output stops, monitoring continues |
| Paused, dwell < 30 s | Hold — no resume regardless of temperature |
| Paused, dwell ≥ 30 s, temp > 39 °C (`IPG_TEMP_THRESHOLD_RESUME`) | Hold — keep waiting |
| Paused, dwell ≥ 30 s **and** temp ≤ 39 °C | `WPT_FAULT_RESUME(PAUSE_THERMAL)` — coil re-enabled |

The dwell is counted in fault-timer ticks (`THERMAL_PAUSE_MIN_TICKS = 15` × 2 s). Both conditions are required: the dwell alone would allow re-enabling into an implant still sitting at the limit, producing a power-burst cycle rather than a controlled hold, while the threshold alone gave no guaranteed cool-down time.

**Two changes from the previous design worth noting:**

*No application-layer involvement.* The old three-tier scheme (41 °C → `TURN_OFF`, 39 °C → `WPT_POWER_OFF`, 36 °C → `WPT_POWER_ON`) sent the charger out of `StateCharge` entirely on the top tier, into a state whose only exit is a physical button press. Worse, `WPT_POWER_OFF` routed through `DisableWpt()`, which stopped the very timer that drives this function — so once paused, temperature was never re-sampled and the resume branch was unreachable dead code. Thermal faults now stay inside the WPT service layer and never touch the application state machine.

*Resume does not reset the power level.* An earlier revision called `ResetPgoodMonitoringStateMachine()` on thermal resume. Under the current design that would jump the level back to its starting point on every recovery, producing 30-second power bursts at the thermal limit. Instead the level is preserved and `m_floor_found` is cleared, re-arming the downward search (§6.3) — the trip is treated as evidence to search lower, not as a reason to start over.

**In debug-build manual mode (§4.7)** the temperature is still computed and logged every 2 s. Crossing 41 °C logs `[MANUAL] WARN thermal: … (ignored)` but sends no `WPT_FAULT_PAUSE`. The IPG's own thermal gate at 42 °C is independent of the charger and still applies.

### 6.6 OVP Handling — and why PGOOD alone is ambiguous

This is the single most important interaction between the two firmwares, and the charger did not participate in it at all before 2026-09-03.

**The mechanism.** On any OVP fault — `VRECT_OVP`, or a per-battery `CHGx_OVP_ERR` — the IPG sets `VCHG_DISABLE` and drops both `CHGx_EN` (`app_mode_wpt.c:285-295`). `VCHG_DISABLE` turns off the VCHG converter, and `VCHG_PGOOD` is that converter's own power-good pin, sampled directly into MSD bit 2 (`app_mode_ble_active.c:154`).

**Therefore an OVP pause drives PGOOD to 0.** PGOOD = 0 means either *too little power* or *too much power*, and those require opposite corrections. A controller that reads PGOOD without also reading OVP cannot tell them apart.

The pre-2026-09 charger assumed the first reading and increased power, which raised VRECT further and held the fault asserted. That produced a limit cycle matching the long-observed "collapse" symptom exactly: ramp up → PGOOD = 1 → VRECT near the OVP threshold → trips seconds later → PGOOD = 0 → old state machine resets to minimum power → VRECT collapses → OVP clears → IPG resumes → ramp up → repeat.

**The IPG was written expecting the charger to close this loop.** From `app_mode_wpt.c:291-292`, on the 5 s `WPT_OVP_PAUSE_HOLD_MS` hold:

> *"Hold in PAUSED for OVP events — gives the charger 2–3 BLE advertisement cycles to see VRECT_OVP and reduce coil power before the IPG re-enables and re-triggers the fault."*

**Charger-side handling** (`IpgOvpMonitoring()`, 2 s fault timer):

```cpp
// Raw active-low bits: 0 = asserted. See the warning in §5.3.
const bool ovp_active = (p.GET_VRECT_OVP == 0) || (p.GET_CHG2_OVP_ERR == 0);
```

| Step | Action |
|------|--------|
| On assert | Record `m_ovp_ceiling = min(ceiling, m_level)`, then `WPT_FAULT_PAUSE(PAUSE_OVP)` |
| Dwell < 10 s (`OVP_PAUSE_MIN_TICKS = 5`) | Hold |
| Dwell ≥ 10 s, OVP still asserted | Hold, log at WARNING |
| Dwell ≥ 10 s, OVP cleared | Step level down one, set `m_blank_cycles`, `WPT_FAULT_RESUME(PAUSE_OVP)` |

`CHG1_OVP_ERR` is deliberately **not** part of the trip condition — `CHG1_STATUS` reads 1 in every state observed on this hardware and CHG2 is the battery actually being charged, so including CHG1 would add noise without signal. It is logged for diagnostics only.

**Why the blanking cycle exists.** When the IPG re-enables, its VCHG rail needs a moment to recover, so for one or two samples PGOOD reads 0 while OVP has already cleared. That is precisely the "add power" condition, and acting on it would undo the back-off and re-trip the fault immediately. `BLANK_CYCLES_AFTER_FAULT` makes the power loop skip a cycle after any fault resume.

**Timing note.** The charger's 10 s pause is longer than the IPG's own 5 s hold, so the IPG re-enables first. This is safe: the charger's coil is off throughout its pause, VRECT is at zero, and OVP is therefore guaranteed clear by the time the charger resumes one step lower. It is more conservative than the handshake strictly requires — the IPG's stated expectation is a *power reduction*, not a coil shutdown — at the cost of ~10 s of charging per trip.

**In debug-build manual mode (§4.7)** OVP is still detected and `m_ovp_ceiling` is still recorded, but the charger neither pauses nor steps down; it logs `[MANUAL] WARN ovp: … ignored` instead. The IPG's own response is unaffected — it still sets `VCHG_DISABLE`, so PGOOD still drops. That is why manual mode prints the raw IPG fault bits on every fault tick: it is the only way to tell an IPG protecting itself from a command that did not take effect.

---

## 7. Charging Completion Detection

`ProcessNewBleData()` in `app_state_machine.cpp` is called each time a fresh IPG advertisement arrives. It reads `CHG1_STATUS`, `CHG2_STATUS`, and `VCHG_PGOOD` from the parsed advertisement data:

```cpp
bool chg1_charging = (CHG1_STATUS == 0);   // 0 = nCHRG pulled low = actively charging
bool chg2_charging = (CHG2_STATUS == 0);
bool pgood         = (VCHG_PGOOD == 1);
bool all_done      = !chg1_charging && !chg2_charging;

if (all_done && pgood)
    → WptPort::WPT_BATTERY_CHARGED     (no-op in StateCharging — see §6.4)
    → BlePort::STOP_SCANNING
    → SystemPort::BATTERY_CHARGED
else
    → WptPort::WPT_BATTERY_CHARGING
```

Note the still-charging branch sends `WPT_BATTERY_CHARGING` on the **WPT** port, not a system event — there is no `BATTERY_CHARGING` in `SystemPort::Event_e`. The completion branch sends three events, and only the last of the three is what actually drives the application state machine to `StateWait`.

The `PGOOD` qualifier is essential: when the charger first enters `StateCharge`, the IPG's boost converter may not yet be in regulation and `CHGx_STATUS` can read 1 (idle) before the charger ICs have actually started. Requiring `PGOOD = 1` ensures that a charge-complete reading is only accepted when the converter is confirmed active.

The LTC4065 charger ICs in the IPG handle their own charge termination (C/10 cutoff) and autonomous re-charge — the charger firmware never instructs them directly. `CHGx_STATUS` reading 1 means only that the IC's `nCHRG` pin is floating (charge complete or not yet started); combined with `PGOOD = 1`, it reliably indicates charge complete.

---

## 8. Change History

### 8.1 — 2026-05 baseline

| File | Summary of changes |
|------|-------------------|
| `src/application_layer/state_machine/state_charge.h` | Added `#include "svc_wpt_manager.h"` and `WptManager &mWptManager` private member |
| `src/application_layer/state_machine/state_charge.cpp` | `Entry()` now unconditionally sends `WPT_POWER_ON` and calls `StartIpgTemperaturePgoodMonitoringTimer()`; `BLE_SCAN_TIMEOUT` restarts scanning instead of exiting the state |
| `src/application_layer/state_machine/state_slow_charge_and_scan.cpp` | Scan timeout changed to `SCAN_TIMEOUT_SLOW_CHARGE_MS` (30 s); `WPT_POWER_OFF` not sent on `BLE_DEVICE_FOUND` (WPT left enabled for `StateCharge::Entry()` to take over); PGOOD monitoring timer started on device found; `PMC_POWER_ON` added to `Entry()` to enable 5V VCC rail; `PMC_POWER_OFF` added to `BLE_SCAN_TIMEOUT` and `WPT_SCAN_TIMEOUT` cases |
| `src/service_layer/ble/svc_ble_manager.h` | `SCAN_TIMEOUT_MS` changed from 2,000 to 10,000 ms; `SCAN_TIMEOUT_SLOW_CHARGE_MS` (30,000 ms) added; `CARSS_COMPANY_ID` changed from `0xFFFF` to `0xF0F0` |
| `src/service_layer/ble/svc_ble_manager.cpp` | Advertisement parser updated to new IPG MSD format (thermistors as uint8 × 10 mV, GPIO bits packed in single byte, battery voltages from msd[1]/msd[2]) |
| `src/service_layer/wpt/state_machine/svc_wpt_state_charging.cpp` | `WPT_SLOW_CHARGE` case added (clamped DAC to step 0 — superseded, see §8.3); `WPT_BATTERY_CHARGED` made a deliberate no-op (shutdown routed through app layer to avoid race) |

| Constant | Old value | New value | Reason |
|----------|-----------|-----------|--------|
| `SCAN_TIMEOUT_MS` | 2,000 ms | 10,000 ms | More time for the IPG to respond between advertising intervals |
| `SCAN_TIMEOUT_SLOW_CHARGE_MS` | (new) | 30,000 ms | IPG needs ~10–20 s to boot from off state via WPT |
| `CARSS_COMPANY_ID` | `0xFFFF` | `0xF0F0` | Match production IPG firmware company ID |

### 8.2 — 2026-08 thermal pause auto-resume

Charging paused at 41 °C but never resumed. Every overtemp trip sent `WPT_POWER_OFF` → `DisableWpt()`, which stopped the monitoring timer that drives `IpgTemperatureMonitoring()` — so temperature was never re-sampled and the resume branch was unreachable. The 41 °C path also sent `SystemPort::TURN_OFF`, dropping the application state machine to `StateWait`, which recovers only by button press.

Fixed by decoupling "stop delivering power" from "stop monitoring": dedicated pause/resume events that toggle only the coil driver, handled without any state transition. Thresholds were simplified from three tiers (41/39/36) to a single hysteresis band (41/39). Superseded in structure by §8.3, but the decoupling principle carries forward unchanged. The commit (`0ed080a`) also checked in a build artifact, `Firmware/hornet-wpt-charger_Debug_260804.hex`.

### 8.3 — 2026-09-03 bidirectional OVP-aware power control

Addresses two field failures: a powered-off IPG never waking at minimum coil power, and the PGOOD "collapse" limit cycle described in §6.6.

| File | Summary of changes |
|------|-------------------|
| `src/service_layer/wpt/svc_wpt_manager.h` / `.cpp` | Monitoring timer split into `mFaultTimer` (2 s) and `mPowerCtrlTimer` (10 s); `PgoodState` machine and `IpgPgoodMonitoring()` replaced by the rule-based `PowerControlMonitoring()`; `IpgOvpMonitoring()` added; pause-reason bitmask with `PauseWpt()`/`ResumeWpt()` replacing `PauseWptForThermal()`/`ResumeWptFromThermal()`; `EnableWpt()` now honors the pause mask; `DisableWpt()` calls `ResetPowerControl()`; floor/ceiling bounds and empty-window detection added; `StartColdStartEscalation()` added; 30 s thermal dwell added |
| `src/service_layer/ble/svc_ble_manager.h` / `.cpp` | `mAdvertisementCounter` and `GetAdvertisementCount()` added for telemetry validity and freshness; `SCAN_TIMEOUT_SLOW_CHARGE_MS` shortened |
| `src/service_layer/wpt/svc_wpt_port.h` | `WPT_THERMAL_PAUSE`/`WPT_THERMAL_RESUME` renamed to `WPT_FAULT_PAUSE`/`WPT_FAULT_RESUME` at the same values, now carrying a `PauseReason_e` in `optDataAddress` |
| `src/service_layer/wpt/state_machine/svc_wpt_state_charging.cpp` | Fault cases forward the reason; `WPT_SLOW_CHARGE` now starts cold-start escalation instead of clamping to step 0 |
| `src/service_layer/wpt/state_machine/svc_wpt_state_slow_charge.cpp` | Same fault-case changes, mirrored |

| Constant | Old value | New value | Reason |
|----------|-----------|-----------|--------|
| `TEMP_PGODD_MONITOR_PERIOD_MS` | 2,000 ms | *(split)* | One rate could not serve both loops |
| `FAULT_MONITOR_PERIOD_MS` | (new) | 2,000 ms | The IPG holds its OVP pause only 5 s |
| `POWER_CTRL_PERIOD_MS` | (new) | 10,000 ms | A power step must settle and propagate through a BLE advertisement |
| `SCAN_TIMEOUT_SLOW_CHARGE_MS` | 30,000 ms | 10,000 ms | Cold start now uses high power, so the attempt either works quickly or not at all |
| `IPG_TEMP_THRESHOLD_HIGH/MEDIUM/LOW` | 41 / 39 / 36 °C | *(removed)* | Replaced by a single pause/resume band |
| `IPG_TEMP_THRESHOLD_PAUSE` / `_RESUME` | (new) | 41 / 39 °C | Single hysteresis band |
| `STABILITY_THRESHOLD`, `MAX_FINE_TUNE_STEPS`, `MAX_COUNT_TOGGLING` | 10 / 2 / 3 | *(removed)* | Sized for the deleted state machine and the 2 s cadence |

Two logging-only commits also landed on this branch. `2eb194b` (before the above) made `IpgTemperatureMonitoring()` log the IPG temperature to two decimal places and added a raw `GET_THERM_REF/OUT/OFST` line. `68d90a0` (after it) renamed the resistance log line in `CalculateTemperatureFromBle()` from "IPG thermal resistance" to "IPG thermistor resistance". Neither changes behaviour.

### 8.4 — 2026-09-09 manual/debug control mode over RTT

Adds a bench-test mode so the control loop of §8.3 can be validated against a hand-driven
ground truth. Off by default and absent from a production binary: everything is gated on
`WPT_MANUAL_DEBUG_MODE` in `src/service_layer/debug/svc_debug_config.h`, and a debug build
still boots and runs the normal automatic loop until the operator presses `m` over RTT.

Manual mode is an **application-layer state**, `StateManual`, not a flag that carves
exceptions out of the automatic states. The first cut was flag-based and failed on
hardware in three ways, all traceable to that choice:

1. It never raised VCC_EN, so the console drove `PIN_WPT_EN` into an unpowered LTC4125.
   The 5 V rail is owned solely by the PMC state machine (`PmcStateEnable::Entry/Exit`),
   and only `StateCharge::Entry()` and `StateSlowChargeAndScan::Entry()` ever request it.
2. `DebugConsole::StartCoil()` sent `WPT_FAULT_RESUME` before `WPT_POWER_ON`, but WPT
   `StateIdle` drops the former, so a set `PAUSE_MANUAL` could never be cleared.
3. It gated `StateCharge` but not `StateSlowChargeAndScan`, whose `BLE_SCAN_TIMEOUT`
   handler is the only one in the codebase that sends `WPT_POWER_OFF` **and**
   `PMC_POWER_OFF` **and** `ChangeState(pStateWait)` — killing power ~10 s in.

The third is the instructive one: the bug was a *missing exception*, which is the failure
mode a flag-based design invites and a state-based one cannot have. Behaviour that must
not happen in manual mode is now expressed as the absence of a case in
`StateManual::DispatchEvent`, and behaviour that must happen is `Entry()`/`Exit()`.

Seven conditional gates collapsed to two:

| Original gate | Now |
|---|---|
| `StateCharge` `BATTERY_CHARGED` | never a transition in `StateManual` — handled only to re-arm scanning |
| `StateCharge` `WPT_SCAN_TIMEOUT` | no such case |
| `StateSlowChargeAndScan` power-off (the missed one) | not in that state |
| `PowerControlMonitoring` | `mPowerCtrlTimer` is never started |
| `StartColdStartEscalation` / `ColdStartEscalate` | never started in manual mode; one left pending by the previous state is cancelled on entry (`EnterManualIdle()`) |
| `IpgTemperatureMonitoring` warn-only | **kept** — the monitor must run and report |
| `IpgOvpMonitoring` warn-only | **kept** — same |

The two survivors are genuinely conditional: manual mode changes whether those monitors
*act*, not whether they *run*.

**This does not make the system unprotected.** The IPG runs its own thermal gate at 42 °C
and its own OVP shutdown, neither of which the charger can disable (§6.6). That is why
manual mode prints the IPG's raw fault bits on every 2 s fault tick — without that line,
an IPG protecting itself is indistinguishable from a command that never took effect. A
dead-man timeout pauses the coil after 10 minutes with no keystroke, covering the one risk
the IPG's own gate does not: a bench rig with no IPG present.

| File | Summary of changes |
|------|-------------------|
| `src/service_layer/debug/svc_debug_config.h` | New. `WPT_MANUAL_DEBUG_MODE` (default 0), poll period, dead-man timeout |
| `src/service_layer/debug/svc_debug_console.{h,cpp}` | New. 100 ms RTT poll timer, single-keystroke handling, status dump, dead-man timer, `IsManual()`; `OnManualEntered()`/`OnManualExited()` so `StateManual` owns the manual flag |
| `src/application_layer/state_machine/state_manual.{h,cpp}` | New. `Entry()` raises VCC_EN, starts scanning and sends `WPT_MANUAL_IDLE`; ignores Button 1; re-arms scanning after `BATTERY_CHARGED`; `Exit()` stops the coil, scanning and the rail |
| `src/application_layer/app_port.h` | `MANUAL_TAKEOVER = 0x12`. `SystemPort::ExecuteEvent` dispatches straight to the state machine and never indexes `mEventCallback`, so `MAX_EVENT_ENUM_LENGTH` does not constrain it as it does on the BLE and WPT ports |
| `src/application_layer/state_machine/app_state_machine.{h,cpp}` | `StateManual` registered in `StatePointers` and the constructor |
| `state_wait.cpp`, `state_scan.cpp`, `state_slow_charge_and_scan.cpp`, `state_charge.cpp` | One `MANUAL_TAKEOVER` case each → `ChangeState(pStateManual)` |
| `src/service_layer/wpt/svc_wpt_manager.{h,cpp}` | `PAUSE_MANUAL` in `PauseReason_e`; `StartFaultMonitoringOnly()`; `EnterManualIdle()`; `m_coil_enabled` + `IsCoilEnabled()`; `RearmPowerSearch()` (currently unused, §6.2); warn-only thermal/OVP paths; IPG-bit logging in `FaultMonitoring()`; accessors for the status dump |
| `src/service_layer/wpt/svc_wpt_port.h` | `WPT_MANUAL_IDLE = 0x11` (debug builds) |
| `svc_wpt_state_idle.cpp`, `svc_wpt_state_charging.cpp`, `svc_wpt_state_slow_charge.cpp` | `WPT_MANUAL_IDLE` case → `EnterManualIdle()`; the two active states also → `StateIdle` (debug builds) |
| `src/service_layer/wpt/state_machine/svc_wpt_state_slow_charge.cpp` | **Bug fix, independent of debug mode:** added the missing `WPT_ADJUST_POWER` case. Power adjustments arriving in this state were previously discarded and the DAC kept its prior value |
| `src/hal_layer/hal_wpt.h` | `StepToMillivolts()` so a PTH step can be reported in volts without duplicating the range constants |
| `src/project/project/hornet-wpt-charger.emProject` | New `debug` folder, include directory, and `state_manual.cpp` |

Design notes worth preserving:

- **One new WPT port event, deliberately.** `MAX_EVENT_ENUM_LENGTH` is 20 and `WptPort::Event_e`
  already reached `0x10`. PTH changes reuse `WPT_ADJUST_POWER`; coil stop/start reuses
  `WPT_FAULT_PAUSE`/`WPT_FAULT_RESUME` with `PAUSE_MANUAL`. The one addition,
  `WPT_MANUAL_IDLE` (0x11), exists because entry has to be ordered after the previous
  state's queued WPT events (§4.7).
- **`StateManual::Entry()` drives the WPT side to `StateIdle` rather than inheriting it.**
  The coil is off because of where that state machine is, not because of a pause bit
  something else might clear. `s` is what moves it to `StateCharging`.
- **Command ordering in `StartCoil()` is load-bearing**: `WPT_POWER_ON` →
  `WPT_FAULT_RESUME` → `WPT_ADJUST_POWER`. The latter two are dropped by `StateIdle`.
- **A manual stop is a pause, not `WPT_POWER_OFF`.** Power-off transitions to `StateIdle`
  and calls `DisableWpt()` → `ResetPowerControl()`, discarding the state being observed.
  `x` therefore leaves VCC_EN up, so only `WPT_ENn` moves between stop and start.
- **`StateManual::Exit()` must send `PMC_POWER_OFF` itself** — `StateCharge::Exit()` does
  not, which is why leaving `StateCharge` normally parks VCC_EN high.
- **`IsCoilEnabled()` is not `GetPauseReasons() == 0`.** In `StateIdle` the mask is zero and
  the coil is off, which is exactly where manual mode idles before `s`.

| Key | Action | Key | Action |
|-----|--------|-----|--------|
| `m` / `n` | enter / exit manual mode | `s` / `x` | coil start / stop |
| `+` / `-` | step PTH up / down | `0`-`9`, `a`, `b`, `c` | set PTH step 0-12 |
| `?` | status dump | `h` | help |

Single keystrokes were chosen so commands fit the existing 16-byte RTT down-buffer; no
`sdk_config.h` change is required.

**Latent trap, not currently live:** `EnableWpt()` starts `mStatusTimeoutTimer` (5 s), whose
callback sends `WPT_SCAN_TIMEOUT` if `GetStat() == 1`, and `StateCharging` answers that with
`WPT_POWER_OFF`. In manual mode that would be a silent kill. It cannot fire today because
`Wpt_LTC4125::GetStat()` is stubbed to `return 0` (§11). If HSD-285 is ever resolved and
`GetStat()` implemented, this path must be revisited.

**Status (2026-09-10).** Documenting the `StateManual` design turned up four defects by inspection: three entry paths that did not meet the "coil off, monitoring on" contract, and a stale console flag after leaving with Button 1. The same day, manual mode was changed to override every operational state explicitly — stopping any charge in progress and idling until `s` — with entry routed through `WPT_MANUAL_IDLE`, Button 1 disabled, and the flag owned by the state (§4.7). Both flag states compile clean across all 53 first-party sources. Not yet run on hardware; the only hardware run so far was the flag-based first cut described above.

---

## 9. Event Flow — Complete Happy Path

### 9.1 IPG already advertising (awake)

```
User presses button
  → OnOffButtonCallback() → SystemPort::BUTTON_PRESSED
  → App SM: StateWait → StateScan
  → StateScan::Entry(): BlePort::START_SCANNING, blue LED on
  → BleManager::StartScanning(), 10 s timer started

IPG advertisement received (company ID 0xF0F0)
  → BleManager::ParseManufacturerSpecificData()
  → BlePort::DEVICE_FOUND (from ISR)
  → App SM: StateScan → StateCharge
  → StateScan::Exit(): WptPort::WPT_POWER_ON (WPT SM: StateIdle → StateCharging), blue LED off
  → StateCharge::Entry():
      PmcPort::PMC_POWER_ON
      yellow LED on
      WptPort::WPT_POWER_ON (no-op — WPT SM already in StateCharging)
      mWptManager.StartIpgTemperaturePgoodMonitoringTimer()
  → StateCharging::Entry(): mWptManager.EnableWpt() (WPT_EN LOW, starts status timers)

Every 2 s (mFaultTimer): IpgOvpMonitoring() + IpgTemperatureMonitoring()
  → OVP asserted:  record ovp_ceiling, WPT_FAULT_PAUSE(PAUSE_OVP)
                   after 10 s and clear: step down one, resume
  → temp ≥ 41°C:   WPT_FAULT_PAUSE(PAUSE_THERMAL)
                   after 30 s and ≤ 39°C: resume, re-arm the floor search
  (Neither leaves StateCharge — the yellow LED stays on throughout)

Every 10 s (mPowerCtrlTimer): PowerControlMonitoring()
  → skipped while any fault is pending, blanked, or telemetry is stale
  → first cycle with telemetry: set step 7
  → PGOOD=1, floor not found: step down
  → PGOOD=1, floor found:     hold
  → PGOOD=0 twice running:    step up, record pgood_floor, floor found

IPG advertisement: CHG1_STATUS=1, CHG2_STATUS=1, PGOOD=1
  → ProcessNewBleData() → SystemPort::BATTERY_CHARGED
  → App SM: StateCharge → StateWait
  → StateCharge::Exit(): BlePort::STOP_SCANNING, WptPort::WPT_POWER_OFF, yellow LED off
  → green LED on
```

### 9.2 IPG powered off (slow charge path)

```
User presses button
  → StateScan::Entry(): blue LED, BLE scanning (10 s)

No IPG found — BLE_SCAN_TIMEOUT fires
  → StateScan → StateSlowChargeAndScan
  → StateSlowChargeAndScan::Entry():
      SetScanTimeout(10 s)
      BlePort::START_SCANNING
      WptPort::WPT_SLOW_CHARGE     ← cold start: LTC4125 at step 7
      white LED on

After 5 s with no advertisement
  → ColdStartEscalate(): LTC4125 to maximum power for the remaining 5 s

WPT powers IPG; IPG boots, starts advertising
  → BleManager receives advertisement
  → BlePort::DEVICE_FOUND
  → App SM: StateSlowChargeAndScan → StateCharge
      WPT and VCC left enabled (no WPT_POWER_OFF or PMC_POWER_OFF sent)
  → StateSlowChargeAndScan::Exit():
      SetScanTimeout(10 s), white LED off
  → StateCharge::Entry():
      PMC_POWER_ON (no-op — PMC already in PmcStateEnable), yellow LED
      WptPort::WPT_POWER_ON       ← transitions WPT SM from any state to StateCharging
      StartIpgTemperaturePgoodMonitoringTimer()

First 10 s control cycle with telemetry
  → level snaps to step 7, closed loop takes over from the cold-start escalation

... charging proceeds as normal ...
```

### 9.3 Manual bench control (debug builds only)

```
Debug build, board in any operational state (idle, scanning, slow charge, charging)

Operator presses 'm' over RTT
  → DebugConsole::EnterManual(): request only
  → SystemPort::MANUAL_TAKEOVER
  → App SM: <any operational state> → StateManual
      (the previous state's Exit() runs first and may queue WPT events)
  → StateManual::Entry():
      DebugConsole::OnManualEntered() ← m_manual = true
      PmcPort::PMC_POWER_ON          ← VCC_EN high; LTC4125 powered
      BlePort::START_SCANNING        ← telemetry for the warn-only monitors
      WptPort::WPT_MANUAL_IDLE       ← processed after the previous state's WPT events
      magenta LED
  → WPT task, EnterManualIdle():
      DisableWpt()                   ← coil off, timers stopped, escalation cancelled,
                                       level → step 7
      StartFaultMonitoringOnly()     ← fault timer only; no titration, no cold start
      WPT SM → StateIdle             ← coil off until 's'

Operator presses 's'
  → PmcPort::PMC_POWER_ON            (no-op — already in PmcStateEnable)
  → WptPort::WPT_POWER_ON            ← WPT SM: StateIdle → StateCharging → EnableWpt()
  → WptPort::WPT_FAULT_RESUME(PAUSE_MANUAL)
  → WptPort::WPT_ADJUST_POWER(level)
    (order matters: StateIdle would drop the last two)

'+' / '-' / '0'…'9', 'a', 'b', 'c'
  → WptManager::SetPowerLevelManual() → WPT_ADJUST_POWER → DAC CH1

'x'
  → WPT_FAULT_PAUSE(PAUSE_MANUAL)    ← coil off; VCC_EN stays up; stays in StateCharging

Every 2 s (mFaultTimer)
  → thermal / OVP thresholds evaluated, logged as [MANUAL] WARN, never acted on
  → [MANUAL] IPG bits: … printed every tick

Every 10 s with no IPG (BLE_SCAN_TIMEOUT)
  → StateManual re-sends START_SCANNING — no state change, power unaffected

10 min with no keystroke (dead-man)
  → WPT_FAULT_PAUSE(PAUSE_MANUAL) — coil off until the operator presses 's'

Button 1
  → ignored in StateManual (logged); Button 2 still resets the MCU

IPG reports battery charged (ProcessNewBleData() sends STOP_SCANNING)
  → StateManual re-sends START_SCANNING on BATTERY_CHARGED

Operator presses 'n'
  → DebugConsole::ExitManual(): request only
  → SystemPort::TURN_OFF
  → App SM: StateManual → StateWait   (always StateWait, never the previous state)
  → StateManual::Exit(): OnManualExited() (m_manual = false), stop monitoring
                         timers, WPT_POWER_OFF, STOP_SCANNING, PMC_POWER_OFF, LED off
```

---

## 10. Constants Reference

**`svc_ble_manager.h`:**
```cpp
SCAN_INTERVAL              = 0x00A0
SCAN_WINDOW                = 0x0050
SCAN_TIMEOUT_MS            = 10000     // ms — StateScan and StateCharge
SCAN_TIMEOUT_SLOW_CHARGE_MS = 10000   // ms — StateSlowChargeAndScan
CARSS_COMPANY_ID           = 0xF0F0   // Production IPG company ID
```

**`svc_wpt_manager.h`:**
```cpp
// Timers
FAULT_MONITOR_PERIOD_MS        = 2000     // ms — temperature + OVP sampling
POWER_CTRL_PERIOD_MS           = 10000    // ms — power search step interval
COLD_START_ESCALATE_MS         = 5000     // ms — step 7 → maximum, open loop

// Thermal (single hysteresis band + dwell)
IPG_TEMP_THRESHOLD_PAUSE       = 41       // °C — pause at/above
IPG_TEMP_THRESHOLD_RESUME      = 39       // °C — resume at/below
THERMAL_PAUSE_MIN_TICKS        = 15       // fault ticks (× 2 s) = 30 s dwell

// OVP
OVP_PAUSE_MIN_TICKS            = 5        // fault ticks (× 2 s) = 10 s dwell

// Power search
MIN_POWER_LEVEL                = 0        // PTH step
COLD_START_LEVEL               = 7        // PTH step — cold start and loop entry
LEVEL_INVALID                  = 0xFF     // sentinel: no bound observed yet
LEVEL_REQUEST_MAX              = 0xFE     // out of range on purpose; HAL clamps
PGOOD_LOW_CONFIRM              = 2        // consecutive PGOOD=0 cycles before stepping up
BLANK_CYCLES_AFTER_FAULT       = 1        // control cycles skipped after a fault resume
```

**`hal_wpt.h`:**
```cpp
VoltageMinPulseWidthThreshold_mV  = 400
VoltageMaxPulseWidthThreshold_mV  = 1600
VoltageStepPulseWidthThreshold_mV = 100
MAX_VOLTAGE_STEP_PULSE_WIDTH      = 12
NTC_R0                            = 5000     // Ω — local NTC at 25°C
NTC_BETA                          = 3480
```

**`svc_debug_config.h`** (debug console — see §4.7):
```cpp
WPT_MANUAL_DEBUG_MODE      = 0         // 1 = compile the console and StateManual in
WPT_MANUAL_POLL_PERIOD_MS  = 100       // ms — RTT keystroke poll
WPT_MANUAL_DEADMAN_MS      = 600000    // ms — pause the coil after 10 min idle; 0 disables
```

---

## 11. Known Limitations and Future Considerations

- **`WPT_SLOW_CHARGE` in `StateSlowCharge`:** The WPT state `StateSlowCharge` exists in the state machine but is not currently reachable through the active code paths — `StateSlowChargeAndScan` sends `WPT_SLOW_CHARGE` while the WPT SM is in `StateCharging` (already enabled) rather than `StateIdle`, because `StateScan::Exit()` sends `WPT_POWER_ON` first. The `StateSlowCharge` code is preserved for potential future use, and carries the fault pause/resume cases so it stays correct if it ever becomes reachable.

- **CTD control path is asserted but never used:** See §2.1. `WPT_CTD_CTRL` (P0.05) is driven HIGH at initialization and never changed, holding LTC4125 `CTD` at GND for the whole session. `StopSearch()` and `ResumeSearch()`, the functions meant to control it, are empty stubs — so `StopWptScan()` (reached via `WPT_STOP_SCAN`) does nothing. Needs a datasheet check and a scope on pin 14 to determine whether the current static state is correct.

- **The WPT status path is inert:** `Wpt_LTC4125::GetStat()` unconditionally returns 0 (`hal_wpt.cpp:206-213`, TODO HSD-285 tracks the IC response investigation). Consequently `StatusTimeoutMonitoring` can never fire `WPT_SCAN_TIMEOUT`, the `WPT_SCAN_TIMEOUT → StateWait` transitions documented in §4.5 and §4.6 are unreachable, and the 500 ms `mStatusTimer` produces log output only. The WPT-side timeout safety net does not currently exist. If it is ever made live it becomes a silent coil kill in debug-build manual mode — `EnableWpt()` arms the 5 s timer and `StateCharging` answers `WPT_SCAN_TIMEOUT` with `WPT_POWER_OFF` — so revisit that path if HSD-285 is resolved.

- **`TURN_OFF` is unsent in production builds:** Nothing outside the debug console sends it, so the `state_charge.cpp:60` handler is unreachable in a production build. It is no longer safe to delete — debug builds use it to leave `StateManual` (§4.7).

- **Partial fault recovery:** Thermal and OVP faults now recover automatically (§6.5, §6.6). `WPT_FAULT_CONDITION` does not — it still sends `WPT_POWER_OFF` and falls to `StateIdle` with no retry and no user notification. A future enhancement could flash the LED or attempt a power cycle.

- **Local NTC is unused *and* its conversion is broken:** `WPT_NTC` (AIN6) measures the charger PCB / transmit coil temperature. Two independent problems:

  1. **No control logic.** `mWptNtcTemperature` is written once in `hal_wpt.cpp:166` and read once in `app_system.cpp:74` inside a `LOG_INFO` on the 5 s heartbeat. There is no threshold, comparison, event, or state transition anywhere — the coil could overheat with no firmware response.
  2. **The reading is meaningless.** `NtcVoltageToTemperature()` (`hal_wpt.cpp:193`) computes `log_R = (int32_t)(log(R_NTC / 5000.0) * 1)` — multiplying by 1 where the following line's `/1000` shows 1000 was intended — then truncates to an integer. For any resistance ratio between 1/e and e the result truncates to 0, which collapses the next line to `T0_mK` exactly and returns a hardcoded **25 °C**. With `NTC_R0 = 5000` and `NTC_BETA = 3480` that covers roughly 3–47 °C, i.e. the entire realistic operating range.

  So there is currently **no charger-side thermal limit of any kind**, and the value in the logs is a constant. Since the coil sits against the patient's skin during charging, a touch-temperature limit is normally a requirement rather than an enhancement (IEC 60601-1 §11.1) — and fixing the conversion is a prerequisite for implementing one. Two hardware facts also need confirming before trusting a corrected value: whether the NTC is the high- or low-side element in the divider (`hal_wpt.cpp:189` assumes low-side), and whether `VCC_mV = 5000` (`hal_adc.h:23`) matches what the SAADC actually sees, since the nRF52840 cannot take 5 V directly and there is presumably a divider ahead of AIN6.

- **Cold start is open-loop:** Between 5 s and 10 s of the cold-start window the charger drives maximum PTH with no PGOOD or OVP telemetry to steer by. A drained IPG loads VRECT down hard, so OVP is unlikely — but this is the one window in which a fault could not be detected. Worth watching on a bench unit with VRECT instrumented.

- **VCC_EN stays high after leaving `StateCharge`:** `StateCharge::Exit()` sends no `PMC_POWER_OFF` (§4.6), so after a completed charge or a button press the LTC4125 remains powered, though disabled, in `StateWait`.

- **Stale comment on the slow-charge scan window:** `state_slow_charge_and_scan.cpp:29` still says the state uses "a longer scan timeout so the IPG has time to power up". `SCAN_TIMEOUT_SLOW_CHARGE_MS` has equalled `SCAN_TIMEOUT_MS` (10 s) since §8.3 deliberately shortened it.

- **`WptManager::RearmPowerSearch()` has no callers** (§6.2) — kept deliberately, but dead code today.

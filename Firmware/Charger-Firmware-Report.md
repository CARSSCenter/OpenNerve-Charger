# WPT Charger — Firmware Implementation Report

**Hardware:** Hornet WPT Charger PCBA
**MCU:** nRF52810 (via nRF5 SDK)
**Repo path:** `Firmware/Source-Code/`

---

## 1. Overview

The charger firmware manages the complete wireless power transfer (WPT) charging session from user button press to battery full. It communicates with the IPG entirely through BLE advertisement scanning — the IPG broadcasts its charging status, thermistor readings, and GPIO state in a Manufacturer-Specific Data (MSD) payload every second while charging, and the charger uses that data to make power and thermal decisions.

The firmware is built on an **event-driven architecture (EDA)** framework. The application layer, BLE service, WPT service, and PMC service each run their own state machines and communicate exclusively by sending events to each other's ports. No state machine inspects another's internal state directly.

---

## 2. Hardware Overview

### 2.1 WPT Controller — LTC4125

The LTC4125 is the wireless power transmitter IC. In normal operation, LTC4125 autonomously runs an "Optimum Power Search (OPS)" algorithm to find the power level at which the receiver (IPG) is best coupled, using the reflected impedance from the secondary coil to determine best coupling. However, this firmware operates differently; by actively driving the `PTH1`/`PTH2` pins via a 16-bit SPI DAC (DAC80504), the pulse width / power transfer is directly controlled (igher DAC voltage → larger allowed duty cycle → more power delivered). There are 13 DAC steps allowed which each correspond to an increasing power transmission level; when an IPG is present, an algorithm increases this power level until the PGOOD signal in the IPG indicates that sufficient power is being delivered to enable battery charging.  See **6.3 PGOOD-Based Adaptive Power Control** for details on this algorithm. 

| DAC voltage | Step | Approximate power |
|-------------|------|-------------------|
| 400 mV | 0 (minimum) | Minimum — slow charge / coil presence detection |
| 500 mV | 1 | — |
| … | … | — |
| 1600 mV | 12 (maximum) | Maximum |


**CTD pin:** Not driven. With CTD unconnected the LTC4125 retries its OPS cycle continuously at ~20 ms intervals. Grounding CTD would stop the search after the first cycle — the opposite of what is needed for continuous operation.

### 2.2 DAC — DAC80504

16-bit quad-channel SPI DAC. Only CHANNEL_1 is used to drive `PTH1`/`PTH2`. Operating at `VREF = 2500 mV` with gain = 1×. The `LDAC` pin is toggled after each SPI write to latch the new value.

```
DAC voltage = 400 mV + (step × 100 mV)    step ∈ [0, 12]
```

### 2.3 Key GPIO / Analog Pins

| Signal | Pin | Direction | Notes |
|--------|-----|-----------|-------|
| `WPT_EN` | P1.15 | Output | HIGH = LTC4125 enabled |
| `WPT_STAT` | P0.7 | Input | LTC4125 load-detection status |
| `WPT_NTC` | AIN6 | Analog | Local NTC thermistor on charger PCB |
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

---

## 3. Firmware Architecture

### 3.1 Layer Structure

```
┌─────────────────────────────────────────────┐
│              Application Layer              │
│  app_state_machine.cpp / app_system.cpp     │
│  state_wait / state_scan / state_charge /   │
│  state_slow_charge_and_scan /               │
│  state_initialization                       │
├─────────────────────────────────────────────┤
│              Service Layer                  │
│  BLE: svc_ble_manager / svc_ble_subsystem   │
│  WPT: svc_wpt_manager / svc_wpt_subsystem   │
│  PMC: svc_pmc_subsystem                     │
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
| `BATTERY_CHARGING` | At least one battery still charging |
| `BATTERY_CHARGED` | All batteries full |
| `BUTTON_PRESSED` | User pressed the power button |
| `BUTTON_DFU_PRESSED` | DFU mode button combo |
| `TURN_OFF` | Thermal protection: shut down |

**WPT service events** (`svc_wpt_port.h`):

| Event | Meaning |
|-------|---------|
| `WPT_POWER_ON` | Enable LTC4125, begin OPS |
| `WPT_POWER_OFF` | Disable LTC4125 |
| `WPT_SLOW_CHARGE` | Clamp DAC to minimum power (step 0) |
| `WPT_ADJUST_POWER` | Set specific DAC step |
| `WPT_BATTERY_CHARGING` | BLE confirms charging in progress |
| `WPT_BATTERY_CHARGED` | BLE confirms all batteries full |
| `WPT_SCAN_TIMEOUT` | Power search timed out |
| `WPT_FAULT_CONDITION` | Fault detected |

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
   │  (Yellow LED)    │    │   WPT at minimum power     │
   └──────┬───────────┘    │   BLE scanning (30 s)      │
          │                └──────────┬─────────────────┘
          │                          │ BLE_DEVICE_FOUND
          │                          │
          │    ◄─────────────────────┘
          │
          │ BATTERY_CHARGED → StateWait (green LED)
          │ BUTTON_PRESSED  → StateWait
          │ TURN_OFF        → StateWait
          │ WPT_SCAN_TIMEOUT → StateWait
```

### 4.2 StateInitialization

Entry sends `INITIALIZE` to the BLE, WPT, and PMC subsystem ports. When `BLE_INITIALIZED` is received, transitions to `StateWait`. This is the only state where subsystem initialization occurs.

### 4.3 StateWait

Idle state. No LEDs, no scanning, no WPT. Transitions to `StateScan` on `BUTTON_PRESSED`.

### 4.4 StateScan

```
Entry:
  BlePort::START_SCANNING
  hal::Leds::LedScanning(true)     ← blue LED

DispatchEvent:
  BLE_DEVICE_FOUND:
    mWptManager.StartIpgTemperaturePgoodMonitoringTimer()
    → ChangeState(StateCharge)

  BLE_SCAN_TIMEOUT:
    BlePort::STOP_SCANNING
    WptPort::WPT_SLOW_CHARGE       ← enables WPT at minimum power
    → ChangeState(StateSlowChargeAndScan)

  BUTTON_DFU_PRESSED:
    hal::Dfu::start_dfu_mode()

Exit:
  hal::Leds::LedScanning(false)
```

When no IPG is found within the 10-second scan window, the charger assumes the IPG may be powered off. It enters `StateSlowChargeAndScan`, enabling WPT at minimum power so the IPG can boot from wireless power, then scans for another 30 seconds to catch the BLE advertisement after initialization.

### 4.5 StateSlowChargeAndScan

```
Entry:
  BleManager::SetScanTimeout(30000)     ← 30 s window for IPG boot time
  BlePort::START_SCANNING
  WptPort::WPT_SLOW_CHARGE              ← LTC4125 at minimum power
  hal::Leds::LedChargingSlow(true)      ← white LED

DispatchEvent:
  BLE_DEVICE_FOUND:
    mWptManager.StartIpgTemperaturePgoodMonitoringTimer()
    → ChangeState(StateCharge)          ← WPT stays enabled

  BLE_SCAN_TIMEOUT:
    BlePort::STOP_SCANNING
    mWptManager.StopIpgTemperaturePgoodMonitoringTimer()
    WptPort::WPT_POWER_OFF
    → ChangeState(StateWait)

  WPT_SCAN_TIMEOUT:
    WptPort::WPT_POWER_OFF
    → ChangeState(StateWait)

  BUTTON_DFU_PRESSED:
    hal::Dfu::start_dfu_mode()

Exit:
  BleManager::SetScanTimeout(10000)     ← restore default
  hal::Leds::LedChargingSlow(false)
```

**Key design decision — WPT state on BLE_DEVICE_FOUND:** When the IPG is detected, WPT is left enabled intentionally. `StateCharge::Entry()` sends `WPT_POWER_ON` to ensure the WPT service is in `StateCharging` regardless of which path led here. This avoids the unnecessary disable/enable round-trip that would briefly cut power to the still-booting IPG.

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
    → ChangeState(StateWait)            ← thermal shutdown path

  BLE_SCAN_TIMEOUT:
    BlePort::START_SCANNING             ← restart scan, do NOT exit charge state

  WPT_SCAN_TIMEOUT:
    → ChangeState(StateWait)

  BUTTON_DFU_PRESSED:
    hal::Dfu::start_dfu_mode()

Exit:
  BlePort::STOP_SCANNING
  WptPort::WPT_POWER_OFF
  hal::Leds::LedCharging(false)
```

**BLE_SCAN_TIMEOUT handling in StateCharge:** Once the IPG detects wireless power, it typically reduces its BLE advertising rate to conserve power. The 10-second scan timeout would otherwise kick the charger out of `StateCharge` and disable WPT. Instead, when a scan timeout fires while in `StateCharge`, BLE scanning is restarted immediately. The charger remains in `StateCharge` and the PGOOD monitoring loop continues uninterrupted. WPT is never disabled on a scan timeout in this state.

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
| `SCAN_TIMEOUT_SLOW_CHARGE_MS` | 30,000 ms | `StateSlowChargeAndScan` |

`SetScanTimeout(ms)` must be called before `StartScanning()` (or between advertisements) to take effect. `StateSlowChargeAndScan::Entry()` calls it with 30,000 ms; `StateSlowChargeAndScan::Exit()` restores 10,000 ms.

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

**Thermistor bytes** are in units of 10 mV; the parser multiplies by 10 to produce mV for `CalculateTemperatureFromBle()`.

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

StateSlowCharge
  │ WPT_POWER_ON → StateCharging
  │ WPT_POWER_OFF → StateIdle

StateCharging
  │ Entry: mWptManager.EnableWpt()
  │ WPT_POWER_ON: no-op (already active)
  │ WPT_POWER_OFF → StateIdle (calls mWptManager.DisableWpt())
  │ WPT_SLOW_CHARGE: mWptManager.AdjustWptPowerTransfer(0)
  │ WPT_ADJUST_POWER: mWptManager.AdjustWptPowerTransfer(step)
  │ WPT_BATTERY_CHARGED: no-op (handled by app layer — see §6.4)
  │ WPT_SCAN_TIMEOUT: → WPT_POWER_OFF
  │ WPT_FAULT_CONDITION: → WPT_POWER_OFF

StateTest (not implemented)
```

### 6.2 WPT Manager

`WptManager` is the singleton that owns the LTC4125 and DAC directly.

**`EnableWpt()`:**
1. Sets `WPT_EN` HIGH
2. Sets DAC CHANNEL_1 to step 0 (400 mV)
3. Starts the status monitoring timer (500 ms periodic)

**`DisableWpt()`:**
1. Sets DAC CHANNEL_1 to 0 V
2. Sets `WPT_EN` LOW
3. Stops the status monitoring timer

**`AdjustWptPowerTransfer(step)`:**
Calls `SetPulseWidthThresholdStep(step)`, which writes:
```
voltage_mV = 400 + (step × 100)
dac_code = (voltage_mV / 2500.0) × 65535
```
to DAC CHANNEL_1 and latches via `LDAC`.

### 6.3 PGOOD-Based Adaptive Power Control

The WPT manager runs a power search state machine in `IpgPgoodMonitoring()`, called every 2 seconds by `mTempPgoodTimer`. The goal is to find the minimum DAC step at which `VCHG_PGOOD` (from the IPG BLE advertisement) is reliably high, then lock in that level.

**States:**

```
INIT
  └── Set step = 0 → INCREASING_POWER

INCREASING_POWER
  ├── PGOOD = 1 → save stable_step → STABILIZING
  └── step == MAX (12): reset to step 0, stay in INCREASING_POWER

STABILIZING
  ├── PGOOD = 1 for 10 consecutive readings → STABLE (or FINE_TUNING)
  └── PGOOD = 0: back to INCREASING_POWER

FINE_TUNING
  ├── Try step - 1 or step - 2 (smaller = more efficient)
  ├── If PGOOD toggles > 3 times: revert to stable_step → STABLE
  └── If stable at new step: update stable_step → STABLE

STABLE
  └── PGOOD = 0: back to INCREASING_POWER
```

**Key constants:**

```cpp
STABILITY_THRESHOLD  = 10   // consecutive PGOOD=1 readings required to declare stable
MAX_COUNT_TOGGLING   = 3    // PGOOD toggles before aborting fine-tune
MAX_VOLTAGE_STEP     = 12   // maximum DAC step
```

This algorithm ensures the LTC4125 transmits at the minimum power needed to keep the IPG's boost converter in regulation, rather than running at full power throughout the session.

### 6.4 Battery Charged Handling — Race Condition Avoidance

When the IPG reports all batteries full (`CHG1_STATUS = 1` AND `CHG2_STATUS = 1` AND `VCHG_PGOOD = 1`), the application layer sends `BATTERY_CHARGED` to the system port. `StateCharge::Exit()` then sends `WPT_POWER_OFF` through the normal port path.

`StateCharging::DispatchEvent` receives `WPT_BATTERY_CHARGED` but deliberately does **not** send `WPT_POWER_OFF` directly. Doing so would race with the `SlowChargeAndScan → Charge` transition: multiple `DEVICE_FOUND` events can be queued while `STOP_SCANNING` is still pending, and a second `WPT_BATTERY_CHARGED` would disable WPT after `StateCharge::Entry()` has already re-enabled it via `WPT_POWER_ON`. Routing all shutdown through the application layer's `StateCharge::Exit()` avoids this.

### 6.5 IPG Temperature Monitoring

`IpgTemperatureMonitoring()` runs on the same 2-second periodic timer as the PGOOD loop. It reads `GET_THERM_REF`, `GET_THERM_OUT`, and `GET_THERM_OFST` from the BLE advertisement and calculates IPG temperature using the same 104AP-2 NTC lookup table used by the IPG firmware:

| Temperature | Resistance |
|-------------|-----------|
| 20°C | 126,400 Ω |
| 25°C | 100,000 Ω |
| 30°C | 79,590 Ω |
| 40°C | 51,290 Ω |
| 50°C | 33,790 Ω |

**Thermal actions:**

| Condition | Action |
|-----------|--------|
| temp ≥ 41°C | `SystemPort::TURN_OFF` — exits `StateCharge` |
| 39°C ≤ temp < 41°C | `WptPort::WPT_POWER_OFF` — pauses charging |
| temp < 36°C (recovery) | `WptPort::WPT_POWER_ON` — resumes charging |

---

## 7. Charging Completion Detection

`ProcessNewBleData()` in `app_state_machine.cpp` is called each time a fresh IPG advertisement arrives. It reads `CHG1_STATUS`, `CHG2_STATUS`, and `VCHG_PGOOD` from the parsed advertisement data:

```cpp
bool chg1_charging = (CHG1_STATUS == 0);   // 0 = nCHRG pulled low = actively charging
bool chg2_charging = (CHG2_STATUS == 0);
bool pgood         = (VCHG_PGOOD == 1);
bool all_done      = !chg1_charging && !chg2_charging;

if (all_done && pgood)
    → SystemPort::BATTERY_CHARGED
else
    → SystemPort::BATTERY_CHARGING
```

The `PGOOD` qualifier is essential: when the charger first enters `StateCharge`, the IPG's boost converter may not yet be in regulation and `CHGx_STATUS` can read 1 (idle) before the charger ICs have actually started. Requiring `PGOOD = 1` ensures that a charge-complete reading is only accepted when the converter is confirmed active.

The LTC4065 charger ICs in the IPG handle their own charge termination (C/10 cutoff) and autonomous re-charge — the charger firmware never instructs them directly. `CHGx_STATUS` reading 1 means only that the IC's `nCHRG` pin is floating (charge complete or not yet started); combined with `PGOOD = 1`, it reliably indicates charge complete.

---

## 8. Files Changed

### 8.1 Modified Files

| File | Summary of changes |
|------|-------------------|
| `src/application_layer/state_machine/state_charge.h` | Added `#include "svc_wpt_manager.h"` and `WptManager &mWptManager` private member |
| `src/application_layer/state_machine/state_charge.cpp` | `Entry()` now unconditionally sends `WPT_POWER_ON` and calls `StartIpgTemperaturePgoodMonitoringTimer()`; `BLE_SCAN_TIMEOUT` restarts scanning instead of exiting the state |
| `src/application_layer/state_machine/state_slow_charge_and_scan.cpp` | Scan timeout changed to `SCAN_TIMEOUT_SLOW_CHARGE_MS` (30 s); `WPT_POWER_OFF` not sent on `BLE_DEVICE_FOUND` (WPT left enabled for `StateCharge::Entry()` to take over); PGOOD monitoring timer started on device found |
| `src/service_layer/ble/svc_ble_manager.h` | `SCAN_TIMEOUT_MS` changed from 2,000 to 10,000 ms; `SCAN_TIMEOUT_SLOW_CHARGE_MS` (30,000 ms) added; `CARSS_COMPANY_ID` changed from `0xFFFF` to `0xF0F0` |
| `src/service_layer/ble/svc_ble_manager.cpp` | Advertisement parser updated to new IPG MSD format (thermistors as uint8 × 10 mV, GPIO bits packed in single byte, battery voltages from msd[1]/msd[2]) |
| `src/service_layer/wpt/state_machine/svc_wpt_state_charging.cpp` | `WPT_SLOW_CHARGE` case added (clamps DAC to step 0); `WPT_BATTERY_CHARGED` made a deliberate no-op (shutdown routed through app layer to avoid race) |

### 8.2 Key Constant Changes

| Constant | Old value | New value | Reason |
|----------|-----------|-----------|--------|
| `SCAN_TIMEOUT_MS` | 2,000 ms | 10,000 ms | More time for the IPG to respond between advertising intervals |
| `SCAN_TIMEOUT_SLOW_CHARGE_MS` | (new) | 30,000 ms | IPG needs ~10–20 s to boot from off state via WPT |
| `CARSS_COMPANY_ID` | `0xFFFF` | `0xF0F0` | Match production IPG firmware company ID |

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
  → StateScan::Exit(): blue LED off
  → StateCharge::Entry():
      PmcPort::PMC_POWER_ON
      yellow LED on
      WptPort::WPT_POWER_ON
      mWptManager.StartIpgTemperaturePgoodMonitoringTimer()
  → WPT SM: StateIdle → StateCharging
  → StateCharging::Entry(): mWptManager.EnableWpt() (DAC step 0, WPT_EN high)

Every 2 s: IpgPgoodMonitoring() + IpgTemperatureMonitoring()
  → PGOOD=0: increment DAC step, send WptPort::WPT_ADJUST_POWER
  → PGOOD=1 for 10 cycles: lock in stable step
  → temp > 41°C: SystemPort::TURN_OFF → StateCharge → StateWait

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
      SetScanTimeout(30 s)
      BlePort::START_SCANNING
      WptPort::WPT_SLOW_CHARGE     ← LTC4125 on at minimum power
      white LED on

WPT powers IPG; IPG boots (~10–20 s), starts advertising
  → BleManager receives advertisement
  → BlePort::DEVICE_FOUND
  → App SM: StateSlowChargeAndScan → StateCharge
      WPT left enabled (no WPT_POWER_OFF sent)
  → StateSlowChargeAndScan::Exit():
      SetScanTimeout(10 s), white LED off
  → StateCharge::Entry():
      PMC_POWER_ON, yellow LED
      WptPort::WPT_POWER_ON       ← transitions WPT SM from any state to StateCharging
      StartIpgTemperaturePgoodMonitoringTimer()

... charging proceeds as normal ...
```

---

## 10. Constants Reference

**`svc_ble_manager.h`:**
```cpp
SCAN_INTERVAL              = 0x00A0
SCAN_WINDOW                = 0x0050
SCAN_TIMEOUT_MS            = 10000     // ms — StateScan and StateCharge
SCAN_TIMEOUT_SLOW_CHARGE_MS = 30000   // ms — StateSlowChargeAndScan
CARSS_COMPANY_ID           = 0xF0F0   // Production IPG company ID
```

**`svc_wpt_manager.h`:**
```cpp
TEMP_PGODD_MONITOR_PERIOD_MS   = 2000     // ms — monitoring timer period
IPG_TEMP_THRESHOLD_HIGH        = 41       // °C — shut off WPT
IPG_TEMP_THRESHOLD_MEDIUM      = 39       // °C — pause WPT
IPG_TEMP_THRESHOLD_LOW         = 36       // °C — resume WPT
STABILITY_THRESHOLD            = 10       // consecutive PGOOD=1 readings
MAX_COUNT_TOGGLING             = 3        // PGOOD toggles before aborting fine-tune
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

---

## 11. Known Limitations and Future Considerations

- **`WPT_SLOW_CHARGE` in `StateSlowCharge`:** The WPT state `StateSlowCharge` exists in the state machine but is not currently reachable through the active code paths — `StateSlowChargeAndScan` sends `WPT_SLOW_CHARGE` while the WPT SM is in `StateCharging` (already enabled) rather than `StateIdle`. The `StateSlowCharge` code is preserved for potential future use.

- **CTD pin not implemented:** The LTC4125 `CTD` pin is left unconnected (NC). If a future PCB revision adds a GPIO connection, CTD can be used to further control OPS cycle timing. The current behavior (unconnected, continuous retries at ~20 ms) is correct for the use case.

- **No fault recovery:** `WPT_FAULT_CONDITION` in `StateCharging` sends `WPT_POWER_OFF` and falls to `StateIdle`. There is no automatic retry or fault notification to the user. A future enhancement could flash the LED or attempt a power cycle.

- **Local NTC not used:** `WPT_NTC` (AIN6) measures the temperature of the charger PCB itself. This value is read by `hal::Wpt` but is not currently wired into any thermal protection logic in the service or application layer. Adding a charger-side temperature limit would protect the transmit coil in high-ambient conditions.

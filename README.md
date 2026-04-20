🔥 T-962 V2 STM32 Reflow Oven Firmware (Stock Hardware)
Overview
I Flashed the Firmware in STM32CubeIDE with a stlinkv3minie debug pins are exposed on the 20 pin header there is a picture 
of the header pinout in the media

This project is a clean-room firmware implementation for the T-962 V2 reflow oven using the stock STM32 control board.

The goals of this firmware are:

Reliability and safety
Clean modular architecture
Usable, intuitive interface
Accurate temperature control using stock hardware
Community-friendly foundation for future improvements
✅ Key Features
Core Functionality
Closed-loop temperature control with cold-junction compensation
Support for left/right thermocouple channels
Filtered sensor readings for stable control
Profile-based reflow execution
Manual temperature hold mode
Control Behavior
Full power heating until target is first reached
Duty-based modulation after target for stable hold
Deadband control to prevent oscillation
Cooling fan activation above configurable threshold
Heat and fan interlock (never fight each other)
User Interface
Menu-driven UI (no page scrolling confusion)
Clear separation between:
Normal operation
Service / calibration
Always-visible oven temperature
Built-in and custom profiles
Manual control mode
Profiles
Built-in profiles:
SN63/Pb37 (Leaded)
SN60/Pb40 (Leaded)
SAC305 (Lead-Free)
Low-temp Lead-Free
One user-editable custom profile (stored in EEPROM)
Safety Features
Over-temperature cutoff
Sensor fault detection
No-temperature-rise timeout
Run-screen lock during heating
Deterministic behavior after power loss
Storage
EEPROM-backed settings and profiles
CRC-protected data
A/B redundancy for fault tolerance
Safe defaults on corruption
🧠 Architecture

The firmware is modular and structured:

main.c         → boot + init + main loop
app.c          → system coordination
board.c        → hardware abstraction
sensors.c      → ADC + filtering
temps.c        → temperature math
control.c      → heating/cooling logic
profiles.c     → profile execution
storage.c      → EEPROM handling
ui.c           → display + input
faults.c       → fault detection/handling
Design Rules
No temperature math in UI
No control logic in UI
Hardware isolated from application logic
Deterministic behavior preferred over complexity
🎛️ User Interface
Main Menu
Built-In Profiles
Custom Profile
Manual Target
Built-In Profiles
Select common solder profiles
Automatically loads into working profile
Custom Profile
Run Custom Profile
Edit Profile (steps, temps, durations)
Manual Mode
Set target temperature
Hold temperature indefinitely
In-place editing with accelerated input
🔧 Service Menu (Hidden)

Access:

Hold Up + Down

Contains:

Diagnostics
Calibration
Manual outputs
Settings
Factory Reset
🧪 Diagnostics

Displays:

Raw ADC values
Filtered readings
Computed temperatures
Averaged oven temperature
Fault status

Optimized for:

Debugging sensors
Verifying control behavior
Tuning profiles
⚙️ Control Strategy
Heating
100% output until target reached
After first target hit:
duty-based modulation maintains temperature
Cooling

Fan activates above:

target + cool_margin
Profile Timing

Timer starts when:

temp ≥ (target - deadband)
💾 EEPROM Layout
Versioned blocks
CRC validation
A/B redundancy
Safe fallback defaults

Stores:

Settings
Calibration
Custom profile
🧰 Calibration

Supports:

Left probe offset/gain
Right probe offset/gain
Cold junction adjustment

Calibration is:

Stored in EEPROM
Applied in temps.c
Never handled in UI
🔄 Factory Reset

Available in Service menu.

Resets:

Settings
Calibration
Custom profile

Also:

Aborts active runs
Resets control state
Clears faults
⚡ Hardware Compatibility
Stock T-962 V2 hardware only (initial release)
STM32F103-based board
No hardware mods required
📊 Resource Usage

Typical build:

Flash: ~23 KB / 128 KB
RAM: ~4.5 KB / 20 KB

Plenty of headroom for future features.

🧪 Real-World Notes
Center of oven is hotter than edges
Best results:
place boards under heating elements
avoid edges for critical work
Paste volume and board cleanliness affect results more than firmware at this stage
🚀 Future Improvements

Potential v2 features:

Graph display (temperature vs time)
SD card support
Bluetooth / remote monitoring
Multi-profile storage
Enhanced control (PID or hybrid)
⚠️ Disclaimer

This firmware controls a device capable of:

High temperatures
Electrical hazards

Use at your own risk.

Always:

Monitor initial runs
Verify calibration
Test with scrap boards first
🤝 Contributing

Contributions are welcome:

Bug fixes
Profile improvements
UI enhancements
Documentation
📦 Release Notes (v1.0 RC)
Stable control system
Clean menu-driven UI
Built-in + custom profiles
Manual mode
Diagnostics and calibration
EEPROM reliability improvements
Safety features implemented

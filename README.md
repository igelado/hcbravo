# HoneyComb Bravo Throttle XPlane 12.x Plugin

An XPlane 12.X plugin to drive the knobs and LEDs on the HoneyComb Bravo Throttle.

## Introduction

There are a few XPlane plugins for the HoneyComb Bravo Throttle, but none of them fulfill
my needs.

The official plugin (AFC Bridge) includes a GUI to configure profiles on the
Alpha and Bravo controllers, but it seems like a cumbersome task and it requires enabling
the profiles on the external application whenever you load an aircraft.

There a LUA based plugin that has a bunch of hardcoded aircrafts, so adding a new profile
requires updating the plugin.

This plugin is inspired by the LUA based plugin, but I decided to develop in C++ to minimize
the performance hit of the plugin. Instead of hardcoding profiles for different aircrafts,
this plugin uses YAML configuration files to define different profiles for knobs and LEDs,
so adding support for a new aircraft is as straightforward as adding a new configuration
file into the `conf` directory of the plugin.

## Aircraft Profile Configuration Format

Configuration files are YAML files a **MUST** have a `.yaml` extension.

### Configuration File Structure

The YAML file has three compulsory labels (`name`, `models`, `aircrafts`, and `system`) and two optional labels (`autopilot` and `annunciator`).

The `name` label is a scalar that contains a string with the name of the profile, e.g., `Cessna 172SP`.
The plugin uses this string to identify the profile, but its value **does not** affect how profiles are selected for a given aircraft.

The `aircrafts` label is an enumeration of concrete aircrafts the profile applies to.
When XPlain loads an aircraft, the plugin tries to match the aircraft's name with an element on this list.
If a match is found, it will use the profile for the aircraft.

The `models` label is an enumeration where each element is a valid ICAO model for which the profile is valid.
The contents of this label is a backup mechanism to assign profiles to aircrafts.
If the plugin does not find a match for an aircraft's name, it uses the list of models to determine which aircraft profiles are available for a given aircraft.

#### XPlane DataRef Labels

Configuration entries to define XPlane DataRefs have a short and a long form.
The short form only applies to scalar boolean or interger values, and it consists of a label whose value is the string that defines the path to the DataRef.
```yaml
course: 'sim/cockpit2/radios/actuators/nav1_obs_deg_mag_pilot'
```

Long form DataRef labels are sequences of maps. Each element in the sequence has a compulsory `key` label, whose value is the string defining the path to the DataRef.
For instance, the following long form would be analogous to the previous example:
```yaml
course:
  - key: 'sim/cockpit2/radios/actuators/nav1_obs_deg_mag_pilot' 
```

The first usage of the long form is to define parameters that are defined by more than one DataRef.
For instance, to know whether any door is open we could use
```yaml
 door_open:
  - key: 'sim/flightmodel2/misc/canopy_open_ratio'
  - key: 'sim/flightmodel2/misc/door_open_ratio'
  - key: 'sim/cockpit2/annunciators/cabin_door_open'
```
In this case, we treat each DataRef as a boolean value, and if any of them is `true`, the label will evaluate to `true`.

The second usage og the long form is to allow complex DataRef entries.
The long form also accepts additional labels to characterize the DataRef:
 - `index` identifies the DataRef as a vector, and specifies the vector index where the value is located.
 - `invert` indicates that when the DataRef is used as a boolean, the result should be negated.
 - `values` a sequence of values that will make the DataRef return `true` when used as a boolean.

The DataRef type is discovered when the configuration is bound to X-Plane, rather
than being declared in YAML.
An explicit `index` selects a supported array accessor when one is available;
otherwise a supported scalar accessor is preferred. Array-only DataRefs default
to index 0 with a warning, and scalar-only DataRefs ignore a configured index
with a warning. Boolean inputs prefer integers when both numeric types support
the selected shape; autopilot dial values require floats.

For instance, the following example indicates that heading mode is enable when the corresponding DataRef takes one of multiple values:
```yaml
  hdg:
   - key: 'sim/cockpit2/autopilot/heading_mode'
     values: 
       - 1
       - 14
```

Another example, is to use a specific index of a vector DataRef to read the current voltage on the electric bus:
```yaml
 volts:
  - key: 'sim/cockpit2/electrical/bus_volts'
    index: 0
```

#### Aircraft Systems Configuration

The `system` label is map that defines the XPlane DataRef the profile uses to obtain system values.

The only required label in the map is `volts`, which identifies the DataRef to query to known when the aircraft electrical system has
voltage, and thus it can start lighting the Bravo throttle annunciators.
The `system` map also has an optional `gear` field to define the DataRef used to know the state of the landing gear.
Obviously, if a given aircraft has fixed gear, this field is not required and the landing gear LEDs will remain off.

The `gear` map requires `left`, `nose`, and `right` entries. Each gear requires
`safe` and `unsafe` DataRef entries, using the short or long form described above.
Its LED is green when `safe` is true, red when `safe` is false and `unsafe` is true,
and off when both are false. Safe takes priority when both are true.

For the Lancair Evolution, each gear has its own locked-down indicator and shares
the aircraft's transit indicator:

```yaml
 gear:
  left:
   safe: 'laminar/n844x/lights/left_gear_down'
   unsafe: 'laminar/n844x/lights/any_gear_in_transit'
  nose:
   safe: 'laminar/n844x/lights/nose_gear_down'
   unsafe: 'laminar/n844x/lights/any_gear_in_transit'
  right:
   safe: 'laminar/n844x/lights/right_gear_down'
   unsafe: 'laminar/n844x/lights/any_gear_in_transit'
```

#### Autopilot Configuration

The autopilot configuration is map with two entries: `dials` and `modes`.
The `dials` entry is itself another map, which selects the DataRef to set the value for each of the positions in the selector knob:
`crs`, `hdg`, `vs`, `alt`.
Each dial, including `ias.value`, must contain exactly one DataRef; offline
validation rejects sequences with multiple entries.
If not specified, the plugin assumes that the DataRef in each of these labels takes a `float` value.
The `ias` label is not directly a DataRef, but a map with two labels pointing to two related DataRefs:
  - `is_mach` a boolean DataRef that is true if the airspeed is reported in Mach values
  - `value` a `float` DataRef that specifies the Indicated Airspeed the autopilot should maintain in KNOTS or MACH, depending on the `is_mach` value

The `modes` entry is also map that identifies the DataRef that indicates whether a given autopilot mode is armed.
All the DataRefs in `modes` are treated as boolean values.

The plugin selects an integer or float accessor from the type published by X-Plane.
This also supports float-only custom DataRefs, such as the Zibo's xlua numbers in
`conf/b738.yaml`. Do not declare `type` in YAML; it is discovered during binding.

The only required label in `modes` is `ap`, which identifies whether the Autopilot is armed or not.
The remaining labels are optional and identifies other potential Autopilot modes that might be armed:
  - `hdg` Heading Mode
  - `nav` Nav Mode (both VOR and GPS)
  - `rev` Reverse course mode
  - `vs` Vertical Speed mode
  - `ias` Indicated Air Speed mode (or Flight Level Change)

#### Annunciators Configuration

The annunciators entry is a map that contains one entry per LED indicator in the Bravo throttle.
As in the case of the autopilot modes, all DataRefs are interpreted as boolean values, and all labels in the `annunciator` map are optional:
 - `master_warn` Master Warning
 - `eng_fire` Engine Fire
 - `oil_low` Low Oil Pressure
 - `fuel_low` Low Fuel
 - `anti_ice` Anti-ice active
 - `starter` Starter engaged
 - `apu` Auxiliary Power Unit active
 - `master_caution` Master Caution
 - `vacuum_low` Low Vacuum
 - `hydro_low` Low Hydraulic Pressure
 - `aux_fuel` Auxiliary Fuel in use
 - `parking_brake` Parking Brake engaged
 - `volt_low` Low Voltage
 - `door_open` Open Door 

 ## Compiling from Source

 We use CMake to compile the plugin in all supported Operating Systems.
 CMake downloads X-Plane SDK 4.3.0 and the other build dependencies through CPM
 during configuration. The SDK archive is verified against a pinned SHA-256 checksum.
 After installing CMake, you only need to configure, build, and pack the project:
 ```
 mkdir build && cd build
 cmake -DCMAKE_BUILD_TYPE=Release ..
 cmake --build .
 cpack
 ```

 After running those commands, there should a `hcbravo.zip` file that contains the plugin, configuration files, and joystick profiles.
 You can unzip that archive into the top-level X-Plane 12 installation directory.


 ## Testing Configuration Files

CMake discovers `conf/*.yaml` and registers a separate parameterized GoogleTest
for each file. Adding or removing a file updates the test list on the next build.
Editing an existing file only requires rerunning its test.

```powershell
ctest --test-dir build -C Release -L configuration --output-on-failure
ctest --test-dir build -C Release -R '^conf.evot$' --output-on-failure
```

The `hcbravo-validate` executable uses the same `conf::validate_file` function
as these tests, backed by the configuration parser used by the plugin:

```powershell
.\build\Release\hcbravo-validate.exe conf\evot.yaml conf\b58.yaml
```

It returns 0 when all files are valid, 1 when any file fails, and 2 when no files
are supplied. Parser diagnostics are written to stderr. Validation checks YAML
structure and DataRef descriptors; DataRef availability and numeric type
compatibility are checked when an aircraft is loaded in X-Plane.

## Diagnosing LED Problems

HCBravo writes diagnostics to `Log.txt` in the X-Plane installation directory,
prefixed with `[HCBravo]`. After an aircraft loads, HCBravo binds the profile on
the first flight-loop callback. This follows the SDK's
[deferred initialization guidance](https://developer.x-plane.com/article/deferredinitialization/).
If binding fails, it logs the missing or incompatible DataRef and stops the flight
loop until a
[DataRefs-added notification](https://developer.x-plane.com/sdk/XPLM_MSG_DATAREFS_ADDED/)
schedules another load on the next flight-loop callback. There are no timed
profile retries; repeated notifications are coalesced while a load is pending.
Notifications do not reload an active profile. Unloading the aircraft cancels
pending binding and notification recovery.
This waits for HCBravo's required DataRefs; it does not guarantee that every
plugin has finished asynchronous initialization. Use
`Plugins > HoneyComb Bravo > Reload Aircraft Profiles` to retry after correcting
a configuration.

Diagnostic categories, runtime checks, and detailed log bodies are compiled only
in Debug builds (`NDEBUG` is not defined). All builds log plugin load/unload and
aircraft enable/disable transitions, including the aircraft name and the selected
profile when enabled. Release builds retain errors and configuration warnings;
the Release menu contains only `Reload Aircraft Profiles`.

Debug builds add `Reload All Plugins` and two actions under
`Plugins > HoneyComb Bravo`:

- `Dump LED State`: reads power, gear, autopilot LED modes, and annunciator inputs,
  then logs the cached HID banks and synchronization status.
- `Dump Autopilot State`: reads the configured modes and dial values, including
  airspeed units and unconfigured entries.

Each selection writes one dump immediately. Dumps include raw DataRef values,
array read counts, accepted values, inversion, and resulting boolean states.
They read all configured inputs, including those normally skipped by boolean
short-circuiting or the electrical power gate. Dumping does not change DataRefs,
send HID reports, bind a profile, or schedule flight-loop work. Re-select an action
to inspect the latest state. There is no periodic diagnostic sampling or timer.

Three independent checkable entries control event-based diagnostics; all start
disabled:

- `Knob Diagnostics`: selector changes, rotation direction, acceleration, and ignored commands.
- `DataRef Diagnostics`: binding metadata and knob dial reads/writes.
- `Configuration Diagnostics`: YAML parsing and configuration/profile registration.

Select an entry to enable or disable it; its check mark shows the current setting.
Knob actions and dial reads/writes log when the command runs. Enable Configuration or DataRef
diagnostics and select `Reload Aircraft Profiles` to inspect parsing or binding.
Profile reloads preserve the settings; a plugin restart resets them.

`logger::diagnostic_scope` exists only in Debug builds. Knob callbacks set the DataRef
category while processing an opted-in command. The destructor restores the
previous mask when the callback returns. LED and autopilot dumps have dedicated
functions; regular LED updates and boolean DataRef reads contain no diagnostic
checks or logging.
Binding logs show accepted `values` and type masks (1 = int, 2 = float,
8 = float array, 16 = int array). LED banks are hexadecimal bytes in this order:
autopilot; gear/master warning/engine fire; annunciators through hydraulic;
auxiliary fuel/parking brake/low voltage/door.

Failed or incomplete HID writes log the returned byte count and HID error text,
and normal updates retry after five seconds. Aircraft unload and plugin shutdown
allow a final all-off write immediately, even during that cooldown.
A successful write confirms that the HID API
accepted the report; observing the physical lights still requires the controller.

 ## Using the Plugin in XPlane

 After installing the plugin, you need to configure HoneyComb Bravo to use the plugin commands for the Autopilot knobs:
  - `HCBravo/Alt`
  - `HCBravo/VS`
  - `HCBravo/HDG`
  - `HCBravo/CRS`
  - `HCBravo/IAS`

Additonally you need to assign the plugin commands to dial knob in the autopilot panel:
  - `HCBravo/INC`
  - `HCBravo/DEC` 

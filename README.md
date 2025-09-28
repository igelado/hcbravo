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

The YAML file has three compulsory labels (`name`, `models`, and `system`) and two optional labels (`autopilot` and `annunciator`).

The `name` label is a scalar that contains a string with the name of the profile, e.g., `Cessna 172SP`.
The plugin uses this string to identify the profile, but its value **does not** affect how profiles are selected for a given aircraft.

The `models` label is an enumeration where each element is a valid ICAO model for which the profile is valid.
The plugin uses the list of models to determine which aircraft profiles are available for a given aircraft.

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
 - `type` identifies the DataRef type. Allowed values are `bool`, `int` and `float`.
 - `index` identifies the DataRef as a vector, and specifies the vector index where the value is located.
 - `invert` indicates that when the DataRef is used as a boolean, the result should be negated.
 - `values` a sequence of values that will make the DataRef return `true` when used as a boolean.

For instance, the following example indicates that heading mode is enable when the corresponding DataRef takes one of multiple values:
```yaml
  hdg:
   - key: 'sim/cockpit2/autopilot/heading_mode'
     type: int
     values: 
       - 1
       - 14
```

Another example, is to use a specific index of a vector DataRef to read the current voltage on the electric bus:
```yaml
 volts:
  - key: 'sim/cockpit2/electrical/bus_volts'
    type: float
    index: 0
```

#### Aircraft Systems Configuration

The `system` label is map that defines the XPlane DataRef the profile uses to obtain system values.

The only required label in the map is `volts`, which identifies the DataRef to query to known when the aircraft electrical system has
voltage, and thus it can start lighting the Bravo throttle annunciators.
The `system` map also has an optional `gear` field to define the DataRef used to know the state of the landing gear.
Obviously, if a given aircraft has fixed gear, this field is not required and the landing gear LEDs will remain off.

#### Autopilot Configuration

The autopilot configuration is map with two entries: `dials` and `modes`.
The `dials` entry is itself another map, which selects the DataRef to set the value for each of the positions in the selector knob:
`crs`, `hdg`, `vs`, `alt`.
If not specified, the plugin assumes that the DataRef in each of these labels takes a `float` value.
The `ias` label is not directly a DataRef, but a map with two labels pointing to two related DataRefs:
  - `is_mach` a boolean DataRef that is true if the airspeed is reported in Mach values
  - `value` a `float` DataRef that specifies the Indicated Airspeed the autopilot should maintain in KNOTS or MACH, depending on the `is_mach` value

The `modes` entry is also map that identifies the DataRef that indicates whether a given autopilot mode is armed.
All the DataRefs in `modes` are treated as boolean values.
The only required label in `modes` is `ap`, which identifies whether the Autopilot is armed or not.
The remaining labels are optional and indentifies other potential Autopilot modes that might be armed:
  - `hdg` Heading Mode
  - `nav` Nav Mode (both VOR and GPS)
  - `rev` Reverse course mode
  - `vs` Vertical Speed mode
  - `ias` Indicated Air Speed mode (or Flight Level Change)

#### Annunciators Configuration

The annunciators entry is a map that contanis one entry per LED indicator in the Bravo throttle.
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
 - `hydro_low` Low Hydrolic Pressure
 - `aux_fuel` Auxiliary Fuel in use
 - `parking_brake` Parking Brake engaged
 - `volt_low` Low Voltage
 - `door_open` Open Door 

 ## Compiling from Source

 We use CMake to compile the plugin in all supported Operating Systems.
 After installing CMake, you only need to configure, build, and pack the project:
 ```
 mkdir build && cd build
 cmake -DCMAKE_BUILD_TYPE=Release ..
 cmake --build .
 cpack
 ```

 After running those comands, there should a `hcbravo.zip` file that contains the plugin and the configuration files.
 You can unzip that archive into the XPlane plugins directory.


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
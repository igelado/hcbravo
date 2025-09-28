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


## More Details To Come

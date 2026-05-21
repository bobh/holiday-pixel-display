# Calendar Triggered Effects Design

## Purpose

This document describes how to trigger specific HolidayDisplay lighting effects from Apple Calendar events on a Mac.

This feature belongs in the current `HolidayDisplay` project, not in the future `AI-First` project. The mechanism is a scheduled runtime control path, not a prompt-to-firmware workflow.

## Core Idea

When a calendar event becomes active on a specific date and time, a small macOS helper detects it and sends the corresponding BLE configuration to the HolidayDisplay device.

The control path is:

```text
Apple Calendar event
  -> macOS helper
  -> BLE write(s)
  -> HolidayDisplay peripheral
  -> effect changes immediately
```

## Why BLE Runtime Control Is The Right Approach

This use case should not require:
- rebuilding firmware
- reflashing the board
- changing Arduino code for each holiday

The current HolidayDisplay project already has a BLE control interface for:
- selecting effects
- changing brightness
- changing speed
- setting colors
- saving configuration

That existing BLE interface is the correct mechanism for calendar-based scheduling.

Relevant project artifacts:
- [BLE_Message_Protocol.md](/Users/bobh/Desktop/Projects/HolidayDisplay/BLE_Message_Protocol.md)
- [HolidayDisplay.ino](/Users/bobh/Desktop/Projects/HolidayDisplay/HolidayDisplay.ino)
- [BLEPeripheral.ino](/Users/bobh/Desktop/Projects/HolidayDisplay/BLEPeripheral.ino)
- [BLECentralMac](/Users/bobh/Desktop/Projects/HolidayDisplay/BLECentralMac)

## Proposed System

### Components

`Apple Calendar`
- user creates a holiday event on a specific date/time

`macOS Calendar Trigger Helper`
- background app or agent on the Mac
- reads calendar events using `EventKit`
- decides when a matching event is active
- maps the event to a HolidayDisplay BLE configuration
- connects to the display and writes the configuration

`HolidayDisplay BLE Peripheral`
- receives effect/config writes
- applies the effect immediately
- optionally persists the config to FRAM

## Event Detection Model

The helper should:
- poll calendar state periodically, e.g. every 60 seconds
- determine whether a matching event is active now
- apply a display change only when the active event changes
- avoid resending the same config repeatedly

The helper should store:
- last event identifier applied
- last timestamp applied
- whether a fallback/default scene should be restored when the event ends

## Recommended Event Format

### V1: Event Title Convention

The simplest format is a calendar event title convention.

Example titles:
- `HolidayDisplay: Halloween`
- `HolidayDisplay: Christmas`
- `HolidayDisplay: Thanksgiving`
- `HolidayDisplay: NewYear`

This is easy to use and easy to parse.

### V2: Event Notes With Parameters

Later, the event notes field can carry structured parameters.

Example:

```text
effect_id=4
brightness=180
speed=90
color_primary=255,140,0
color_secondary=255,40,0
save_config=1
```

This allows one calendar event to specify a full scene configuration rather than just a named holiday.

## Recommended V1 Behavior

Start with a hard-coded mapping table in the macOS helper.

Example:

| Calendar Title | Effect Behavior |
|---|---|
| `HolidayDisplay: Halloween` | Halloween-themed effect preset |
| `HolidayDisplay: Christmas` | Warm white / sparkle preset |
| `HolidayDisplay: Thanksgiving` | Amber candle / ember preset |
| `HolidayDisplay: NewYear` | Brighter sparkle preset |

The helper should:
- read the event title
- look up the preset
- write the corresponding BLE characteristics

## BLE Control Mapping

The helper should use the existing BLE protocol and write only the values needed for the selected preset.

Typical writes:
- `effect_id`
- `brightness`
- `speed`
- `color_primary`
- `color_secondary`
- `flags`
- optionally `save_config`

The exact UUIDs and payload shapes should come from:
- [BLE_Message_Protocol.md](/Users/bobh/Desktop/Projects/HolidayDisplay/BLE_Message_Protocol.md)

## Suggested Operational Flow

### Event Start

1. Poll calendar.
2. Detect an active event matching the `HolidayDisplay:` prefix.
3. If it is different from the last applied event:
   - connect to BLE peripheral
   - write effect/config characteristics
   - optionally write `save_config`
   - record the event as applied

### Event End

When the event is no longer active, choose one of these policies:

1. `Do nothing`
- leave the last holiday effect running

2. `Restore default scene`
- send a default effect configuration

3. `Restore previously saved config`
- use a remembered prior state or a saved preset

Recommended v1 policy:
- `Do nothing`

That keeps the logic simpler.

## macOS Implementation Approach

The best implementation path is a small Swift helper using:
- `EventKit` for Calendar access
- `CoreBluetooth` for BLE

This fits the current codebase because the project already includes:
- a macOS BLE central app
- known BLE characteristics
- working BLE architecture on Apple platforms

The implementation can be either:

1. an extension of the existing macOS BLE central app, or
2. a separate lightweight background helper/agent

Recommended:
- separate helper first, to keep scheduling logic independent of the UI

## Suggested File/Component Structure

Possible new macOS helper structure:

```text
CalendarTriggerMac/
  CalendarTriggerApp.swift
  CalendarMonitor.swift
  EventMapping.swift
  HolidayBLEClient.swift
  TriggerStateStore.swift
```

Responsibilities:

`CalendarMonitor`
- polls current calendar events
- returns the currently active HolidayDisplay event

`EventMapping`
- maps event title or note content to a display preset/config

`HolidayBLEClient`
- reuses or mirrors the BLE write logic already used by the macOS/iOS central code

`TriggerStateStore`
- remembers the last event that was applied

## Example V1 Mapping

Example hard-coded mapping:

```text
HolidayDisplay: Halloween
  effect_id = Halloween effect
  brightness = 160
  speed = 90
  color_primary = orange
  color_secondary = deep red
  save_config = 1

HolidayDisplay: Christmas
  effect_id = warm white sparkle
  brightness = 180
  speed = 70
  color_primary = warm white
  color_secondary = green or red accent
  save_config = 1
```

The actual effect IDs must match the current firmware enum and protocol.

## Failure Handling

The helper should handle:
- no matching calendar event
- Bluetooth off
- peripheral unavailable
- write failure
- repeated polling with no state change

Recommended behavior:
- log the failure locally
- retry on the next poll
- do not repeatedly reconnect if the active event has not changed and the last attempt already failed recently

## Security / Privacy Notes

The helper will need:
- Calendar access permission
- Bluetooth access permission

This is expected on macOS and should be surfaced clearly.

## V1 Scope

### In Scope

- one Mac helper
- Apple Calendar event detection
- title-based event mapping
- hard-coded holiday-to-preset table
- BLE writes to HolidayDisplay

### Out Of Scope

- natural-language AI interpretation of calendar events
- firmware generation from events
- WorkBench programming integration
- event-note freeform parsing
- multiple display devices with different mappings

## Why This Matters

This feature enables:
- scheduled automatic holiday themes
- date-driven display behavior
- “set it and forget it” seasonal control

without introducing unnecessary firmware complexity.

It also creates a clean future bridge to `AI-First`, where AI could eventually help author the preset definitions, but the runtime control path would still remain:

```text
calendar event -> mapped display config -> BLE write
```

## Recommended Next Step

Implement v1 as:

1. a title convention using `HolidayDisplay: <Name>`
2. a macOS helper polling every 60 seconds
3. a hard-coded mapping table
4. BLE writes for the mapped preset

That is the smallest useful version of calendar-triggered holiday effects.

# RelayModule — HDL Buspro-style Relay Subdevice Library

Arduino-framework library (STM32duino / "STM32 Cores" board package) implementing
a multi-channel relay subdevice on a shared RS485 bus using the HDL Buspro
wire protocol. Built on `BusproCore` (frame codec + transport + op-code dispatch)
and `MemoryCore` (STM32 flash journaling/storage).

> **Docs status:** This README reflects the code as of the current `main` branch.
> The wire-level frame format (sync bytes, length field, CRC16/XMODEM) has already
> been empirically verified against real captured HDL Buspro frames in `BusproCore`
> — this library no longer runs on a placeholder frame format.

## Dependencies

- `BusproCore` — `BusproFrame`, `BusproTransport`, `BusproDevice`, `BusproOp`,
  `BusproContext`, `Universal`, `Helpers`
- `MemoryCore` — flash-backed key/value storage (STM32F103 journaling scheme)

Both must be installed alongside this library; `RelayModule.h` includes headers
from both directly.

## Architecture

```
RS485 bus --> BusproTransport (frame boundaries, address filtering)   } BusproCore
                        │ BusproFrame (decoded)
                        v
              RelayModule::process()  --  op-code switch/dispatch
                        │
          ┌─────────────┴─────────────┐
          v                           v
  RelayModule (config/state:    RelayController (on/off + status
  channels, zones, scenes,      + reversing control op-codes)
  device identity — all
  persisted via MemoryCore)
```

`RelayModule` owns device state, flash persistence, and configuration op-codes.
`RelayController` (composed inside `RelayModule`) owns the actual channel
control/status op-codes (`CONTROL_SINGLE`, `CONTROL_REVERSING`).

## Device variants

Channel count and HDL device type code are chosen at **compile time** via a
`#define` near the top of `RelayModule.h` (currently hardcoded to `RELAY4`):

| Define      | Channels | `RELAY_TYPE` |
|-------------|----------|--------------|
| `RELAY1`    | 1        | 5500         |
| `RELAY2`    | 2        | 5501         |
| `RELAY3`    | 3        | 467          |
| `RELAY4`    | 4        | 462          |
| `RELAY6`    | 6        | 426          |
| `RELAY8`    | 8        | 463          |
| `RELAY12`   | 12       | 464          |
| `RELAY16`   | 16       | 466          |
| `RELAY24`   | 24       | 432          |
| `WIRELESS`  | 24       | 6100         |

⚠️ Selection uses `#elifdef`, a C++23 preprocessor directive. Confirm your
toolchain (arm-none-eabi-gcc via PlatformIO `ststm32` platform) actually
supports it — if not, this silently compiles the wrong variant with no error.

`CURTAIN_CHANNEL_COUNT` is always `RELAY_CHANNEL_COUNT / 2` (paired relays
drive one curtain motor's open/close).

## Supported operations

All operations dispatch through `RelayModule::process(const BusproFrame&)`,
split into universal (broadcast, `dstAddress == 0xFFFF`) and addressed
(`dstAddress == deviceAddress_`) commands.

### Universal (broadcast)
| Op-code group | Handler |
|---|---|
| `DEVICE_SEARCH_HDL` (req) | `handleSearchDevice` |
| `DEVICE_REMARK` (read req) | `handleReadDeviceRemark` |

### Addressed — device identity
| Op-code group | Handler |
|---|---|
| `DEVICE_FIRMWARE` | `handleReadFirmware` |
| `DEVICE_HARDWARE` | `handleReadHardware` |
| `DEVICE_FINDIT` | `handleFindDevice` |
| `DEVICE_MAC_ADDRESS` (read/write) | `handleReadMacaddress` / `handleModifyMacaddress` |
| `DEVICE_REMARK` (read/write) | `handleReadDeviceRemark` / `handleModifyDeviceRemark` |

### Addressed — channel configuration
| Op-code group | Handler |
|---|---|
| `CHANNEL_REMARK` (read/write) | `handleReadChannelRemark` / `handleModifyChannelRemark` |
| `CHANNEL_ONDELAY` (read/write) | `handleReadChannelOndelay` / `handleModifyChannelOndelay` |
| `CHANNEL_ONPROTECT` (read/write) | `handleReadChannelOnprotect` / `handleModifyChannelOnprotect` |
| `RELAY_CHANNEL_ENABLE` (read/write) | `handleReadChannelEnable` / `handleModifyChannelEnable` |

### Addressed — zones
| Op-code group | Handler |
|---|---|
| `ZONE_MEMBERS` (read/write) | `handleReadZone` / `handleModifyZone` |
| `ZONE_REMARK` (read/write) | `handleReadZoneRemark` / `handleModifyZoneRemark` |

### Addressed — scenes
| Op-code group | Handler | Status |
|---|---|---|
| `SCENE_READ` | `handleSceneRead` | working |
| `SCENE_MODIFY` | `handleSceneModify` | **stub — empty body, not implemented** |
| `SCENE_REMARK` (read/write) | `handleReadSceneRemark` / `handleModifySceneRemark` | **stub — flash I/O commented out, echoes request only** |
| `SCENE_POWERON_EN` (read/write) | `handleSceneResumeENRead` / `handleSceneResumeENModify` | see known issues |
| `SCENE_POWERON_NUM` (read/write) | `handleSceneResumeNumRead` / `handleSceneResumeNumModify` | see known issues |

### Addressed — curtain (paired-relay channels)
| Op-code group | Handler |
|---|---|
| `CURTAIN_CONFIG` (read/write) | `handleCurtainRead` / `handleCurtainModify` |

### Addressed — channel control (via `RelayController`)
| Op-code group | Handler | Behavior |
|---|---|---|
| `CONTROL_SINGLE` (read) | `handleReadStatusRequest` | returns brightness-encoded on/off for all 4 channels |
| `CONTROL_SINGLE` (write) | `handleSingleChannelControl` | `0x00`→off, `0x64`→on, anything else→off |
| `CONTROL_REVERSING` | `handleReversingControl` | inverted mapping of the above |

## Known issues / incomplete areas

These are tracked so they don't get lost — fix before relying on the
corresponding op-codes in production:

1. **`handleModifyChannelEnable` validation bug.** The guard clause uses `&&`
   where it needs `||`:
   ```cpp
   if (frame.payloadLen != (RELAY_CHANNEL_COUNT + 1) &&
       frame.payload[0] == RELAY_CHANNEL_COUNT)
       return;
   ```
   As written, malformed/short frames are **not** rejected in the normal case,
   and the handler proceeds to read `frame.payload + 1` for `RELAY_CHANNEL_COUNT`
   bytes regardless of actual payload length.

2. **`handleSceneResumeENRead` / `handleSceneResumeNumRead` read `payload[0]`
   after asserting zero payload length.** Both check `payloadLen != 0` and
   return early, then immediately index `frame.payload[0]` in the flash
   address lookup — reading past a payload that was just declared empty.

3. **`uint8_t payload[sceneCount];`** in the same two handlers is a
   variable-length array sized by a runtime member (GCC extension, not
   portable C++, and zero-sized if `sceneCount == 0`). Replace with a
   fixed-size buffer bounded by `MAX_SCENE_ENTRIES`.

4. **`SCENE_MODIFY` is unimplemented** (`handleSceneModify` has an empty body
   — no response is sent, master will time out waiting for an ack).

5. **`SCENE_REMARK` read/write don't touch flash** — the `flash_.read`/
   `flash_.update` calls are commented out in both handlers, so scene remarks
   are not actually persisted or retrievable yet.

6. **`handleReadDeviceRemark` responds to broadcast (`0xFFFF`)** while every
   other handler responds directly to `frame.srcAddress`. Confirm this is
   intentional per HDL convention (remark reads are sometimes broadcast
   responses) — otherwise it should target the requester like everything else.

7. **Brightness-to-relay mapping only recognizes `0x00`/`0x64` explicitly**;
   any other value silently maps to off with no error/log. Fine for a pure
   on/off relay, but worth a comment so it isn't mistaken for a bug later.

## Quick start

```cpp
#include <RelayModule.h>

BusproTransport bus(Serial1, /*dePin=*/PA8);
MemoryCore flash(/* ... */);

const uint8_t relayPins[RELAY_CHANNEL_COUNT] = {PB0, PB1, PB2, PB3};
RelayModule relay(bus, flash, /*sectorAddress=*/0, relayPins);

void setup() {
  Serial1.begin(9600);
  relay.begin();
}

void loop() {
  // pump bus.poll()/dispatch loop, calling relay.process(frame) per BusproCore's
  // dispatch mechanism
}
```

## Flash-backed state

Device identity, channel config (remark/enable/on-delay/on-protect), zone
membership/remarks, and scene data are persisted via `MemoryCore` at a caller
supplied `sectorAddress`. On `begin()`, `firstime()` checks the stored MCU UID
and device type against flash; if either differs (fresh chip or firmware
device-type change), `init()` reinitializes defaults, then `syncValues()`
loads working state from flash into RAM.

## Repo layout (actual, current)

```
src/RelayModule.h/.cpp       -- device state, flash persistence, config op-codes
src/RelayController.h/.cpp   -- on/off + reversing control, status read
docs/CAPTURE_TEMPLATE.md     -- historical: HDL frame-capture worksheet used
                                 during BusproCore wire-format verification
                                 (verification is done; kept for reference)
```

`Relay4R.h`, `SceneStore.h` as a standalone header, and the desktop
`test_stubs/` test suite referenced in older docs do not exist in the current
tree — `library.json`/`library.properties` still point at `Relay4R.h` as the
main include and should be updated to `RelayModule.h`.

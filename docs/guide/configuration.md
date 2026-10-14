# Configuration (lsl_api.cfg)

liblsl reads its settings from an optional configuration file, `lsl_api.cfg`.
Without a file, liblsl uses built-in defaults, and these work for most labs.
Use a file when you need to change network ports, limit or widen stream
discovery, separate labs that share a network, turn on logging, or tune timing.

The code that reads the settings is in `src/api_config.cpp`. The file parser
is in `src/util/inireader.cpp`.

## Where liblsl looks for the configuration

liblsl uses the **first** source below that exists and loads without error.
It reads only that one source. Settings from several files are never merged.

1. Content passed to `lsl_set_config_content()`. No file is read.
2. The file named in the `LSLAPICFG` environment variable.
3. The file passed to `lsl_set_config_filename()`.
4. `lsl_api.cfg` in the current working directory of the program.
5. `~/lsl_api/lsl_api.cfg` in the user's home directory.
6. `/etc/lsl_api/lsl_api.cfg`.
7. If none of these loads: the built-in defaults.

Details per platform:

- **Working directory** (4) is where the program was started from. This is not
  always the folder that contains the program.
- **Home directory** (5): liblsl takes the home directory from `HOME`, then
  `USERPROFILE`, then `HOMEDRIVE` + `HOMEPATH`. On Windows this is usually
  `%USERPROFILE%\lsl_api\lsl_api.cfg`, for example
  `C:\Users\alice\lsl_api\lsl_api.cfg`. Editing it needs no admin rights.
- **System file** (6): on Linux and macOS this is `/etc/lsl_api/lsl_api.cfg`
  and applies to all users. On Windows the same path is relative to the root
  of the current drive, usually `C:\etc\lsl_api\lsl_api.cfg`.
- **`LSLAPICFG`** (2) points to one file for one shell or one program run, for
  example `LSLAPICFG=/tmp/special.cfg ./LabRecorder`, or
  `export LSLAPICFG=/tmp/special.cfg` for all programs started afterwards.

If the file named by `LSLAPICFG` or `lsl_set_config_filename()` cannot be
read, liblsl logs an error and continues with the next location.

If a file exists but contains an error (see [File syntax](#file-syntax)),
liblsl logs an error and tries the next location. If `lsl_set_config_content()`
was given invalid content, liblsl logs an error and falls back to the file
search.

### When the configuration is read

The configuration is read **once per process**, the first time liblsl needs a
setting. In practice this is when the program creates its first stream info,
outlet, inlet or resolver, or calls `lsl_protocol_version()`. After that,
liblsl never reads it again. To apply a changed file, restart the program.

`lsl_set_config_content()` and `lsl_set_config_filename()` (declared in
`include/lsl/common.h`) only work if you call them before any other liblsl
function. Later calls have no effect. Example:

```c
#include <lsl_c.h>

int main(void) {
    lsl_set_config_content("[lab]\nSessionID = lab-7\n[log]\nlevel = -1\n");
    /* ... now create outlets, inlets, resolvers ... */
}
```

## File syntax

The file uses a simple INI format: `[section]` lines, followed by
`key = value` lines.

```ini
; A comment
[ports]
BasePort = 16572

[lab]
KnownPeers = {192.168.1.17, 192.168.1.18}
```

The parser is strict. Keep these rules in mind:

- A comment line must **start** with `;` in the first column. `#` is not a
  comment. Comments at the end of a line are not supported: they become part
  of the value.
- A section line must start with `[` in the first column.
- Every other non-blank line must contain `=`. Empty values (`file =`) are an
  error. To use a default, leave the line out.
- Each key may appear only once per section.
- Keys are **case-sensitive**. A misspelled key (`TimeProbeMaxRtt` instead of
  `TimeProbeMaxRTT`) is silently ignored. Unknown keys and sections are also
  ignored.
- Spaces around keys and values are removed. Quotes are not removed, so do not
  quote values.
- **Booleans are `1` or `0`.** Only `1` means true. Any other value, including
  `true`, means false.
- Numbers use `.` as decimal separator. A value that is not a number is read
  as `0`.
- Lists are written in braces: `{a, b, c}`. A value without braces is read as
  an empty list.

Specify only the settings you want to change. This keeps your file working
when defaults change in later versions.

## Settings

Settings marked *Rarely needed* should only be changed if you know why.

**Settings that must match on all machines in a lab.** liblsl does not check
this, but machines with different values may not find each other's streams:
the `[ports]` settings, `SessionID`, `IPv6`, and the multicast addresses
(`ResolveScope` and the address lists). It is safest to copy the same file to
every machine.

### [ports]

| Key | Default | Meaning |
|---|---|---|
| `MulticastPort` | `16571` | UDP port for stream discovery queries (multicast, broadcast and machine-local). |
| `BasePort` | `16572` | First port of the range that outlets use for their data (TCP) and service (UDP) ports. Resolvers also take a UDP port from this range to receive replies. |
| `PortRange` | `32` | Number of ports in the range, from `BasePort` to `BasePort + PortRange - 1`. Each outlet takes one TCP port and one UDP port here, per IP version. This limits how many outlets fit on one machine. Discovery via `KnownPeers` and on the local machine probes every port in the range, so a larger range makes that slower. |
| `AllowRandomPorts` | `1` | If the range is full, let the operating system pick any free port. Such outlets cannot be found by the range probes and need open firewall ports outside the range. |
| `IPv6` | `allow` | Which IP versions to use: `disable` (IPv4 only), `allow` (IPv4 and IPv6), or `force` (IPv6 only). `disabled`, `allowed` and `forced` are also accepted. Any other value is an error. The default is the same on all platforms. |

### [multicast]

These settings control UDP stream discovery. See
[stream discovery](../internals/discovery.md) for how discovery works.

`ResolveScope` picks which address lists are used and the multicast TTL
(time-to-live: how many routers a packet may cross). Each scope includes all
smaller scopes:

| `ResolveScope` | Adds these addresses | TTL |
|---|---|---|
| `machine` | `MachineAddresses` | 0 |
| `link` | `LinkAddresses`, `FF02:` + `IPv6MulticastGroup` | 1 |
| `site` (default) | `SiteAddresses`, `FF05:` + `IPv6MulticastGroup` | 24 |
| `organization` | `OrganizationAddresses`, `FF08:` + `IPv6MulticastGroup` | 32 |
| `global` | `GlobalAddresses`, `FF0E:` + `IPv6MulticastGroup` | 255 |

IPv4 addresses are dropped if `IPv6 = force`, and IPv6 addresses are dropped
if `IPv6 = disable`. An address that cannot be parsed makes the file invalid.

| Key | Default | Meaning |
|---|---|---|
| `ResolveScope` | `site` | Discovery scope, see the table above. Use the smallest scope that works. |
| `MachineAddresses` | `{127.0.0.1}` | Addresses for discovery on the same machine. liblsl also probes these on every port of the port range, so all local streams are found. |
| `LinkAddresses` | `{255.255.255.255, 224.0.0.1, 224.0.0.183}` | IPv4 broadcast and multicast addresses for the local subnet. |
| `SiteAddresses` | `{239.255.172.215}` | IPv4 multicast addresses for the site. |
| `OrganizationAddresses` | `{}` | *Rarely needed.* IPv4 multicast addresses for the organization scope. |
| `GlobalAddresses` | `{}` | *Rarely needed.* IPv4 multicast addresses for the global scope. |
| `IPv6MulticastGroup` | `113D:6FDD:2C17:A643:FFE2:1BD1:3CD2` | *Rarely needed.* The IPv6 group. liblsl adds the scope prefix (`FF02:`, `FF05:`, ...) itself. |
| `AddressesOverride` | `{}` | *Rarely needed.* If not empty, replaces the whole address list built from `ResolveScope`. |
| `TTLOverride` | `-1` | *Rarely needed.* If 0 or more, replaces the TTL chosen by `ResolveScope`. |
| `ListenAddress` | (empty) | *Rarely needed.* Local address that the outlets' discovery sockets bind to. Empty means all interfaces. |
| `Interfaces` | `{}` | *Rarely needed.* Local interface addresses to join multicast groups on and send queries from. IPv6 addresses need the interface index as scope, for example `fe80::1%3`. Empty means all interfaces the operating system reports. |

### [lab]

| Key | Default | Meaning |
|---|---|---|
| `KnownPeers` | `{}` | Host names or IP addresses of machines that may provide streams. liblsl sends discovery queries directly to each port of the port range on these hosts. This works where multicast and broadcast are blocked. |
| `SessionID` | `default` | Name of a logical recording session. Resolvers only list outlets with the same `SessionID`; inlet recovery does not check it (see [connection recovery](../internals/recovery.md)). This is not a security feature. |
| `ResolveOverTCP` | `0` | *Rarely needed.* If `1`, discovery also connects over TCP to every port of the port range on `127.0.0.1`, on `::1` (unless IPv6 is disabled), and on each `KnownPeers` host. Works through firewalls that block UDP discovery, but is **very slow**. |

### [tuning]

All times are in seconds unless stated otherwise. These settings are
*rarely needed*. See [time synchronization](../internals/time_sync.md) for the
`Time*` settings.

| Key | Default | Meaning |
|---|---|---|
| `UseProtocolVersion` | `110` | Network protocol version. Lower it (for example to `100`) to talk to very old liblsl versions. Values above `110` are reduced to `110`. |
| `WatchdogCheckInterval` | `15.0` | How often an inlet checks whether its connection has stalled. |
| `WatchdogTimeThreshold` | `15.0` | If recovery is enabled, an inlet that has received no data for this long re-resolves the stream. It reconnects only if the outlet has disappeared and one replacement is found. |
| `SyncSendTimeout` | value of `WatchdogTimeThreshold` | For synchronous (blocking) outlets: a consumer that cannot take a sample within this time is disconnected. `0` means wait forever. |
| `MulticastMinRTT` | `0.5` | Minimum wait between multicast discovery waves. |
| `MulticastMaxRTT` | `3.0` | How long to wait for replies to a multicast discovery wave. |
| `UnicastMinRTT` | `0.75` | Extra wait between waves when unicast (`KnownPeers`, machine-local) queries are sent. |
| `UnicastMaxRTT` | `5.0` | How long to wait for replies to unicast and TCP discovery queries. |
| `ContinuousResolveInterval` | `0.5` | Extra pause between waves in continuous (background) resolvers. |
| `TimerResolution` | `1` | Windows only: system timer resolution in ms requested for the program's lifetime. `0` leaves it unchanged. Other values than `1` can increase latency. |
| `MaxCachedQueries` | `100` | How many discovery query results each outlet caches. `0` turns the cache off. |
| `TimeUpdateInterval` | `2.0` | Time between clock offset updates. |
| `TimeUpdateMinProbes` | `6` | Probes that must succeed for an offset update. |
| `TimeProbeCount` | `8` | Probes sent per offset update. |
| `TimeProbeInterval` | `0.064` | Time between probes. |
| `TimeProbeMaxRTT` | `0.128` | Extra time to wait for the last probe replies. |
| `OutletBufferReserveMs` | `5000` | Memory pre-allocated for samples in an outlet, in ms of data (regular-rate streams). |
| `OutletBufferReserveSamples` | `128` | The same, in samples, for irregular-rate streams. |
| `InletBufferReserveMs` | `5000` | Like `OutletBufferReserveMs`, for inlets. |
| `InletBufferReserveSamples` | `128` | Like `OutletBufferReserveSamples`, for inlets. |
| `SendSocketBufferSize` | `0` | TCP send buffer size of outlet connections, in bytes. `0` keeps the OS default. |
| `ReceiveSocketBufferSize` | `0` | TCP receive buffer size of outlet connections, in bytes. `0` keeps the OS default. |
| `SmoothingHalftime` | `90.0` | Default half-time of the time-stamp smoothing post-processing option. |
| `ForceDefaultTimestamps` | `0` | If `1`, outlets ignore time stamps passed by the program and use the current local clock instead. |

### [log]

| Key | Default | Meaning |
|---|---|---|
| `level` | `0` (`1` in builds with `LSL_DEBUGLOG=ON`) | Most detailed level to log: `-3` fatal, `-2` errors, `-1` warnings, `0` info, `1` to `9` more and more detail. Values outside `-3` to `9` are an error. |
| `file` | (not set) | Append log messages to this file. When set, nothing is logged to the terminal. |

Messages logged before the configuration is read use the logging library's
defaults. Some detailed messages only exist in `LSL_DEBUGLOG=ON` builds.

## Recipes

### Change the port range

To move liblsl to ports 3051 to 3067:

```ini
[ports]
MulticastPort = 3051
BasePort = 3052
PortRange = 16
```

Use this on every machine in the lab. For an outlet to be reachable through a
firewall, open TCP and UDP from `BasePort` to `BasePort + PortRange - 1`, and
UDP on `MulticastPort`. A machine that only runs inlets needs the UDP range
open for discovery replies.

### Change the multicast scope

The default scope is `site`. To let streams be found across a campus whose
routers forward multicast:

```ini
[multicast]
ResolveScope = organization
```

To keep discovery inside the local subnet, use `ResolveScope = link`. With
`ResolveScope = machine`, only streams on the same computer are found by
multicast. If your site has rules for multicast addresses, change the address
lists or use `AddressesOverride` and `TTLOverride`.

### Define the local lab

List the lab's machines when multicast and broadcast do not work, or to reach
machines across routers or the internet:

```ini
[lab]
KnownPeers = {192.168.1.17, 137.243.177.26, testing.example.org}
```

To also stop multicast discovery, add `ResolveScope = machine` under
`[multicast]`. Remote hosts must allow the port range through their firewall.

To split one network into independent recording setups, give each setup its
own session name. Machines only see streams with the same `SessionID`:

```ini
[lab]
SessionID = lab-001b
```

Anyone on the network can still see and capture the traffic. Use operating
system and network settings to protect data.

### Discover streams over TCP

If a firewall blocks UDP discovery, liblsl can probe the port range over TCP
as a last resort:

```ini
[lab]
ResolveOverTCP = 1
KnownPeers = {192.168.1.17}
```

Each resolve then makes one connection attempt per port per host, at most 16 at a time.
Closed ports are refused at once, but filtered ports (dropped by a firewall) hold one of the
16 slots until the probe is cancelled after `UnicastMaxRTT`, so with several hosts later ports
may never be probed. Use it only for the local machine or a short `KnownPeers` list. Write `1`,
not `true`.

### Turn on logging

To investigate a problem or attach a log to a bug report:

```ini
[log]
level = 0
file = lsllog.txt
```

Use `level = -1` to see only warnings and errors. Leave out `file` to log to
the terminal.

### Tune for wireless networks

These settings have been found to make time synchronization and discovery
more robust over WiFi:

```ini
[tuning]
TimeProbeMaxRTT = 0.100
TimeProbeInterval = 0.010
TimeProbeCount = 10
TimeUpdateInterval = 0.25
MulticastMinRTT = 0.100
MulticastMaxRTT = 30
```

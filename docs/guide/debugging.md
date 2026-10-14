# Debugging connections

When streams can't be found, or are found but no data arrives, these steps narrow down where
the problem is. The general troubleshooting checklist (firewalls, network adapters, Wi-Fi) is on
the {doc}`LSL network troubleshooting page <lsl:info/network-connectivity>`; this page adds tools
for looking at what liblsl actually sends. The messages and ports are explained in
[stream discovery](../internals/discovery.md) and [data transport](../internals/streamfeed.md).

## Check the basics

- **Versions:** run `lslver` (installed with liblsl) or call `lsl_library_info()` on every
  machine to see which liblsl each program really loads.
- **Configuration:** settings that affect discovery (ports, `SessionID`, IPv6, multicast
  addresses) must be the same on every machine; see [Configuration](configuration.md).
- **Logging:** turn on liblsl's log to see discovery, connection and recovery messages:

  ```ini
  [log]
  level = 2
  file = lsllog.txt
  ```

  Higher levels (up to 9) log more. Some detailed messages exist only in builds with
  `LSL_DEBUGLOG=ON`.

## Watch the network traffic with tcpdump

`tcpdump` shows every LSL packet without taking part in the exchange:

```sh
# discovery and time synchronization (UDP), on the loopback interface
sudo tcpdump -vvnA -i lo udp portrange 16571-16603
# the data connections (TCP)
sudo tcpdump -vvnA -i lo tcp portrange 16572-16603
```

`-vv` prints details, `-n` skips name lookups, `-A` prints the packet contents as text, and
`-i` picks the interface: `lo` is the loopback interface on Linux (`lo0` on macOS); use the
network interface (e.g. `eth0`, `en0`, or `any` on Linux) to watch traffic between machines.
The port numbers are the defaults (`MulticastPort` 16571 and `BasePort` 16572 + `PortRange` 32).

What you should see:

- **Discovery queries** from the resolver, starting with `LSL:shortinfo`, followed by the query
  (e.g. `session_id='default' and name='MyStream'`) and the port and query id to reply to. They go
  to port 16571 (multicast and broadcast) and, for machine-local addresses and `KnownPeers`, to
  each port in the range.
- **Replies** from each matching outlet: the query id and the stream's short XML description,
  sent back to the resolver's port. No reply means the outlet didn't receive the query or the
  query didn't match.
- **Time probes** (`LSL:timedata`) from an inlet to the outlet's service port, and their replies,
  every few seconds while time correction is in use.
- On TCP, the inlet's `LSL:streamfeed/110 <uid>` request and the outlet's `LSL/110 200 OK` response.

Avoid "listening" tools such as `socat UDP-RECV:16571,reuseaddr` for this: they bind to the
discovery port themselves, and the operating system may then deliver unicast queries to them
instead of to an outlet, which changes the behavior you are trying to observe.

## Talk to an outlet by hand

An outlet's TCP data port accepts simple text requests, so you can check whether an outlet
serves data without any LSL program on the receiving side. You need the outlet's host and data
port (`v4data_port` in its stream info).

Ask for the full stream description:

```sh
(printf 'LSL:fullinfo\r\n'; sleep 1) | nc <host> <data port>
```

To receive data, send a streamfeed request. This Python script resolves a stream by name (with
pylsl), prints the outlet's host and data port to stderr and the request to stdout:

```python
"""Print an LSL data request (streamfeed handshake) for a stream, for use with nc.

Usage: python lsl_handshake.py "<stream name>" | nc <outlet host> <data port>
"""
import sys
import xml.etree.ElementTree as ET

import pylsl

# bytes per value for each channel format (pylsl.cf_*); strings have no fixed size
VALUE_SIZES = [0, 4, 8, 0, 4, 2, 1, 8]


def handshake(name: str, max_buffer_seconds: int = 360) -> None:
    info = pylsl.resolve_byprop("name", name, 1, 5.0)[0]
    xml = ET.fromstring(info.as_xml())
    lines = [
        f"LSL:streamfeed/110 {info.uid()}",
        f"Native-Byte-Order: {1234 if sys.byteorder == 'little' else 4321}",
        "Endian-Performance: 0",
        "Has-IEEE754-Floats: 1",
        "Supports-Subnormals: 1",
        f"Value-Size: {VALUE_SIZES[info.channel_format()]}",
        "Data-Protocol-Version: 110",
        f"Max-Buffer-Length: {max_buffer_seconds}",
        "Max-Chunk-Length: 0",
    ]
    host = xml.findtext("v4address") or info.hostname()
    print(f"outlet: {host} port {xml.findtext('v4data_port')}", file=sys.stderr)
    sys.stdout.write("\r\n".join(lines) + "\r\n\r\n")
    sys.stdout.flush()


if __name__ == "__main__":
    handshake(sys.argv[1] if len(sys.argv) > 1 else "Test")
```

```sh
python lsl_handshake.py MyStream > request.txt            # prints e.g. "outlet: myhost port 16573"
(cat request.txt; sleep 10) | nc myhost 16573 | xxd | head   # look at the response
(cat request.txt; sleep 60) | nc myhost 16573 > /dev/null    # a near zero-cost consumer, e.g. to load-test an outlet
```

The `sleep` keeps the connection open: many `nc` versions close it as soon as their input ends.
The response is the status line and headers (`LSL/110 200 OK`, `UID`, `Byte-Order`,
`Suppress-Subnormals`, `Data-Protocol-Version`), a blank line, two test-pattern samples and then
the stream's samples in the [1.10 format](../internals/streamfeed.md).

Things that change the outcome:

- If `Value-Size` doesn't match the stream's channel format, the outlet falls back to data
  protocol 1.00 (`Data-Protocol-Version: 100` in the response), which is slower to produce and
  is refused by synchronous outlets (`505`).
- The outlet takes the requested protocol version from the request line, not from the
  `Data-Protocol-Version` header ([#328](https://github.com/sccn/liblsl/issues/328)).
- `404 Not found` means the UID in the request line isn't this outlet's (e.g. the outlet was
  restarted and has a new UID).

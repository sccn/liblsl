# Clock offset accuracy

An inlet estimates the offset between its clock and the outlet's by exchanging UDP time probes
with the outlet (see [time synchronization](../internals/time_sync.md)). The estimate can only be
as good as the timestamps that each host puts on those probes. This page describes a host
setting that biases it.

## Network adapter interrupt coalescing

Many network adapters hold back the interrupt for a received packet for up to a set time, so that
one interrupt can handle several packets. This is called receive interrupt coalescing (on Windows:
"Interrupt Moderation"). liblsl stamps a time probe when it reads it from the socket, which is
after that delay, so a host with receive coalescing adds a one-sided delay to every probe it
receives.

**The clock offset error is about half of that delay.** On a Raspberry Pi 5 (macb driver,
Linux 6.12), measured against a PTP reference, the error grew by about 0.55 µs per µs of
`rx-usecs`: about 27 µs at the driver's default of 49.6 µs, and within about 3 µs with
`rx-usecs 0`.

- The sign depends on whether the inlet or the outlet runs on that host.
- The reported uncertainty does not reveal it. The round-trip time grows by about the coalescing
  time, but nothing shows that the delay is on one side only.
- It is not constant. With coalescing on, the error shrank when other traffic arrived at the
  host, and disappeared at 78,000 small packets per second.
- Equal delays on both hosts would cancel. This follows from how the offset is computed; it was
  not measured.

### Linux

Show the current setting, and turn receive coalescing off:

```sh
ethtool -c eth0
sudo ethtool -C eth0 rx-usecs 0
```

Some drivers (macb among them) also rewrite the transmit setting when the receive setting is
changed, so give both values: `rx-usecs 0 tx-usecs <the current tx-usecs>`. The change does not
survive a reboot; making it permanent (systemd-networkd, NetworkManager, a udev rule) is up to the
system's administrator.

On the Raspberry Pi 5, `rx-usecs 0` made no measurable difference to CPU load, interrupt rate or
throughput, idle or under load up to line-rate TCP on a 1 Gbit/s link and 78,000 small UDP
packets per second: under load the kernel limits the interrupt rate anyway. Other adapters,
drivers and higher packet rates may behave differently; they were not measured.

Adaptive coalescing (`adaptive-rx on`, on drivers that have it) changes the delay with the
traffic by design. It was not tested.

### Windows and macOS

Windows adapters have the same kind of setting ("Interrupt Moderation" in the adapter's advanced
properties), and the mechanism applies in principle, but its effect on LSL's clock offsets has not
been measured on Windows or macOS.

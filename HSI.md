# HSI fork of AsyncMqttClient

The one MQTT library for the HSI Modbus, DMX and KNX gateways (and, if decided, Morteza 90111).
Use the tag, never a branch:

    lib_deps = https://github.com/HSI-Lighting/async-mqtt-client.git#hsi-v1.0.0

Base: marvinroger/async-mqtt-client `develop` @ e9cc6ca (2026-10-28). It already contains the fix from
fatemiAmir/async-mqtt-client 95414a2 (ping deadline erased by sends), in a fuller form (upstream 6337097,
"Allow recovery from silent MQTT connection loss", which also handles the AsyncTCP timeout path).

HSI patches on top (branch `hsi`):

| # | Where | Bug | Came from |
|---|---|---|---|
| 1 | `_onData` | unknown packet type or empty data -> null-pointer crash | Modbus 90122 lib copy |
| 2 | `connect()` | connect refused at once -> stuck CONNECTING, never reconnects | Modbus 90122 2.0.8 |
| 3 | `AsyncMqttClient.cpp` | one info log line per publish/ack (~1 line/s at CORE_DEBUG_LEVEL 3), partly from the AsyncTCP task; `-D HSI_MQTT_VERBOSE` brings them back | KNX bench 2026-11-13 |

## Connection profile: `#include <HsiMqtt.h>`

`hsiMqttConfigure(client, {host, port, clientId, user, pass, willTopic})` sets the settings every HSI gateway shares
(unified MQTT spec §5.2): keepalive 30 s, clean session, Last Will `{"state":"offline"}` QoS 1 retained on
`<status>/Availability`. Constants: `HSI_MQTT_QOS_IN` (1, subscribe to `In`), `HSI_MQTT_QOS_STATUS` (1),
`HSI_MQTT_OFFLINE`. All strings passed must outlive the client (the library keeps pointers, not copies).

No `dependencies` in `library.json`: each product lists its own AsyncTCP (KNX: mathieucarbou v3.3.2, Modbus:
ESP32Async v3.5.0). Otherwise PlatformIO pulls a second AsyncTCP from the registry next to the product's one.

Rules for the products using it (all learned in the field, see each product's CHANGES):
- call `connect()` only from the main loop / own task, never from a timer callback (DMX/90111: RTC_SW_CPU_RST);
- never block inside the callbacks: they run on the AsyncTCP task (DMX 3954e13);
- `setServer(host, ...)` keeps the pointer, not a copy: keep the string alive (90111, DMX).

To take a newer upstream: merge marvinroger `develop` into `hsi`, keep the two patches, bench-test, new tag.

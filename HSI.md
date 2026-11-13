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

## Moving a product onto this fork (for each product's own agent)

1. `platformio.ini`: replace the old entry (vendored `lib/AsyncMqttClient`, `fatemiAmir/async-mqtt-client`,
   `marvinroger/async-mqtt-client`, or `knolleary/PubSubClient`) with
   `https://github.com/HSI-Lighting/async-mqtt-client.git#<tag>`. Delete a vendored `lib/AsyncMqttClient` folder.
   Keep the product's own AsyncTCP entry. Check `pio pkg list`: exactly ONE AsyncTCP.
2. Replace the product's own `setServer / setClientId / setCredentials / setKeepAlive / setCleanSession / setWill`
   block with one `hsiMqttConfigure(client, {...})` call; the strings it gets must be static (pointers are kept).
3. Subscribe to `In` with `HSI_MQTT_QOS_IN`. Publish `Status/Availability` with `HSI_MQTT_QOS_STATUS`, retained;
   take `rtt_ms` from `onPublish(packetId)` of that publish; before a planned restart publish `HSI_MQTT_OFFLINE`
   (QoS 1, retained) and wait for its `onPublish` (max 1 s), spec §5.4.
4. `publish()` returns 0 when the send queue (default 8 KB, `setMaxQueueSize`) or the heap is short: wait and retry
   or keep the message, never assume it went. Size the queue for the biggest single publish (KNX: status reply).
5. Callbacks run on the AsyncTCP task: copy what you need and hand it to your own task/loop. Never block, never
   wait for an ACK there. A message can arrive in pieces (`index`, `total`): collect them.
6. connect() from your own task or loop() only; a connect gives its result as onConnect / onDisconnect (patch 2
   makes a refused connect report onDisconnect). Add your own attempt time limit (KNX: 20 s -> `disconnect(true)`).
7. Bench before release: >= 30 broker stop/start cycles (gaps 0.5-15 s) all reconnect; outage replay; a planned
   restart shows `offline` at once. KNX's script: `hsi-ops/tests/knx-stress/t_mqtt_lib.py`.

Reference implementation: KNX_Gateway_Integration `firmware/fw_network/src/MQTTTask.cpp` + `Availability.cpp`
(commit 4f9b4e7 and later).

#pragma once
// HSI connection profile: the same MQTT connection settings on every HSI gateway (unified MQTT spec §5.2, §8.2).
// One call instead of each product setting its own keepalive / will / session, so they cannot drift apart again.
//
//   static char host[64], cid[96], user[64], pass[64], will[128];   // MUST outlive the client: AsyncMqttClient keeps
//   ...fill them...                                                 // the POINTERS (setServer, setClientId,
//   hsiMqttConfigure(client, {host, port, cid, user, pass, will});  // setCredentials, setWill), not copies.
//
// Then: connect() from the product's own task or loop() - never from a timer callback; never block inside the
// callbacks (they run on the AsyncTCP task). Availability / Will: HSI_MQTT_OFFLINE, QoS 1, retained.
#include "AsyncMqttClient.h"

#define HSI_MQTT_KEEPALIVE_S   30                        // §5.2: the broker notices a dead gateway in ~45 s
#define HSI_MQTT_QOS_IN        1                         // subscribe to <...>/In with QoS 1 (owner 2026-11-13)
#define HSI_MQTT_QOS_STATUS    1                         // Status/Availability: QoS 1 + retained (§8)
#define HSI_MQTT_OFFLINE       "{\"state\":\"offline\"}"  // the Last Will and the planned-restart message (§5.4)

struct HsiMqttConfig {
  const char* host;       // broker host or IP
  uint16_t    port;
  const char* clientId;   // per product rule, stable across reboots (one connection per gateway, spec D9)
  const char* user;       // nullptr or "" = no login
  const char* pass;
  const char* willTopic;  // <status topic>/Availability; nullptr = no Last Will (not allowed for a gateway)
};

inline void hsiMqttConfigure(AsyncMqttClient& c, const HsiMqttConfig& k) {
  const bool login = k.user && *k.user;
  c.setServer(k.host, k.port);
  c.setClientId(k.clientId);
  c.setCredentials(login ? k.user : nullptr, login ? k.pass : nullptr);
  c.setKeepAlive(HSI_MQTT_KEEPALIVE_S);
  c.setCleanSession(true);                                 // §5.2: the gateways keep no session state
  if (k.willTopic) c.setWill(k.willTopic, HSI_MQTT_QOS_STATUS, true, HSI_MQTT_OFFLINE);
}

/*
 * MecanumB ESP32 radio bridge.
 *
 * Wiring (matches PCB/docs/B板引脚定义_v1.md):
 *   GPIO16 (U2RX) <- STM32 PA9  / USART1_TX
 *   GPIO17 (U2TX) -> STM32 PA10 / USART1_RX        921600 8N1
 *   GPIO4         <- SBUS receiver (optional)      100000 8E2 inverted
 *
 * The bridge forwards MAVLink v2 frames between the STM32 UART and a WiFi
 * UDP ground station, injects its own heartbeat and link checks, and turns
 * SBUS frames into RC_CHANNELS for both the STM32 and the ground station.
 * It never arms the vehicle: arming stays an explicit COMMAND_LONG from the
 * Pi or the ground station.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <common/mavlink.h>

#ifndef BRIDGE_UART_RX
#define BRIDGE_UART_RX 16
#endif
#ifndef BRIDGE_UART_TX
#define BRIDGE_UART_TX 17
#endif
#ifndef BRIDGE_UART_BAUD
#define BRIDGE_UART_BAUD 921600
#endif
#ifndef BRIDGE_LED
#define BRIDGE_LED 2
#endif
#ifndef BRIDGE_SBUS_ENABLED
#define BRIDGE_SBUS_ENABLED 1
#endif
#ifndef BRIDGE_SBUS_RX
#define BRIDGE_SBUS_RX 4
#endif
#ifndef BRIDGE_WIFI_SSID
#define BRIDGE_WIFI_SSID ""
#endif
#ifndef BRIDGE_WIFI_PASS
#define BRIDGE_WIFI_PASS ""
#endif
#ifndef BRIDGE_AP_SSID
#define BRIDGE_AP_SSID "MecanumB-Bridge"
#endif
#ifndef BRIDGE_AP_PASS
#define BRIDGE_AP_PASS "mecanum123"
#endif
#ifndef BRIDGE_UDP_PORT
#define BRIDGE_UDP_PORT 14550
#endif
#ifndef BRIDGE_SYSID
#define BRIDGE_SYSID 2
#endif
#ifndef BRIDGE_COMPID
#define BRIDGE_COMPID 68 /* MAV_COMP_ID_TELEMETRY_RADIO */
#endif

namespace {

constexpr uint32_t kFcTimeoutMs = 3000;
constexpr uint32_t kGcsTimeoutMs = 15000;
constexpr uint32_t kSbusTimeoutMs = 500;
constexpr uint32_t kHeartbeatMs = 1000;
constexpr uint32_t kPingMs = 1000;
constexpr uint32_t kRcPeriodMs = 20;
constexpr uint8_t kMaxPeers = 4;

HardwareSerial &fcPort = Serial2;

WiFiUDP udp;

struct Peer {
  IPAddress ip;
  uint16_t port = 0;
  uint32_t last_rx_ms = 0;
};

Peer peers[kMaxPeers];

uint32_t now_ms = 0;
uint32_t last_fc_heartbeat_ms = 0;
uint32_t last_gcs_rx_ms = 0;
uint32_t last_sbus_ms = 0;
uint32_t last_heartbeat_ms = 0;
uint32_t last_ping_ms = 0;
uint32_t last_rc_ms = 0;
uint32_t last_status_ms = 0;
uint32_t ping_seq = 0;
float ping_rtt_ms = NAN;
bool fc_seen = false;

uint16_t sbus_channels[16] = {0};
bool sbus_failsafe = false;
bool sbus_lost = false;

// SBUS decoder state.
uint8_t sbus_buf[25];
uint8_t sbus_len = 0;

char status_line[50] = {0};

uint32_t age(uint32_t since) { return now_ms - since; }

bool fc_alive() { return fc_seen && age(last_fc_heartbeat_ms) < kFcTimeoutMs; }
bool gcs_alive() { return last_gcs_rx_ms && age(last_gcs_rx_ms) < kGcsTimeoutMs; }

void sendMessage(const mavlink_message_t &msg, bool to_udp);
void sendToUdp(const uint8_t *data, uint16_t len);

void announce(const char *text) {
  strncpy(status_line, text, sizeof(status_line) - 1);
  status_line[sizeof(status_line) - 1] = '\0';
  Serial.printf("[bridge] %s\n", status_line);
  mavlink_message_t msg;
  char payload[50] = {0};
  strncpy(payload, text, sizeof(payload) - 1);
  mavlink_msg_statustext_pack(BRIDGE_SYSID, BRIDGE_COMPID, &msg,
                              MAV_SEVERITY_WARNING, payload, 0, 0);
  sendMessage(msg, true);
}

void registerPeer(const IPAddress &ip, uint16_t port) {
  if (ip == IPAddress(0, 0, 0, 0) || port == 0) return;
  for (Peer &peer : peers) {
    if (peer.port == port && peer.ip == ip) {
      peer.last_rx_ms = now_ms;
      return;
    }
  }
  Peer *slot = nullptr;
  for (Peer &peer : peers) {
    if (peer.port == 0) {
      slot = &peer;
      break;
    }
  }
  if (slot == nullptr) {  // replace the oldest
    slot = &peers[0];
    for (Peer &peer : peers) {
      if (peer.last_rx_ms < slot->last_rx_ms) slot = &peer;
    }
  }
  slot->ip = ip;
  slot->port = port;
  slot->last_rx_ms = now_ms;
  Serial.printf("[bridge] GCS peer %s:%u\n", ip.toString().c_str(), port);
}

void sendToUdp(const uint8_t *data, uint16_t len) {
  for (const Peer &peer : peers) {
    if (peer.port == 0 || age(peer.last_rx_ms) >= kGcsTimeoutMs) continue;
    udp.beginPacket(peer.ip, peer.port);
    udp.write(data, len);
    udp.endPacket();
  }
}

void sendMessage(const mavlink_message_t &msg, bool to_udp) {
  uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
  uint16_t length = mavlink_msg_to_send_buffer(buffer, &msg);
  if (to_udp) {
    sendToUdp(buffer, length);
  } else {
    fcPort.write(buffer, length);
  }
}

void sendHeartbeat() {
  mavlink_message_t msg;
  mavlink_msg_heartbeat_pack(BRIDGE_SYSID, BRIDGE_COMPID, &msg, MAV_TYPE_GENERIC,
                             MAV_AUTOPILOT_INVALID, 0, 0, MAV_STATE_ACTIVE);
  uint8_t buffer[MAVLINK_MAX_PACKET_LEN];
  const uint16_t length = mavlink_msg_to_send_buffer(buffer, &msg);
  fcPort.write(buffer, length);  // keeps the ESP32 lease alive on the STM32
  sendToUdp(buffer, length);
  // A directed broadcast lets a freshly started GCS discover the bridge
  // without knowing its address; only the 1 Hz heartbeat is broadcast.
  udp.beginPacket(IPAddress(255, 255, 255, 255), BRIDGE_UDP_PORT);
  udp.write(buffer, length);
  udp.endPacket();
}

void sendPing() {
  ping_seq++;
  mavlink_message_t msg;
  mavlink_msg_ping_pack(BRIDGE_SYSID, BRIDGE_COMPID, &msg, micros(), ping_seq, 1, 1);
  sendMessage(msg, false);
}

// ---------------------------------------------------------------- MAVLink RX

void handleFcMessage(const mavlink_message_t &msg) {
  if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) {
    if (!fc_seen) {
      fc_seen = true;
      Serial.println("[bridge] STM32 MAVLink up");
    }
    last_fc_heartbeat_ms = now_ms;
  } else if (msg.msgid == MAVLINK_MSG_ID_PING) {
    uint32_t sent = mavlink_msg_ping_get_time_usec(&msg);
    if (sent) ping_rtt_ms = (micros() - sent) / 1000.0f;
  }
  // Everything the flight controller emits is relayed to every live GCS.
  sendMessage(msg, true);
}

void handleGcsMessage(const mavlink_message_t &msg) {
  if (msg.msgid == MAVLINK_MSG_ID_HEARTBEAT) last_gcs_rx_ms = now_ms;
  sendMessage(msg, false);
}

// ---------------------------------------------------------------- SBUS input

uint16_t sbusToRc(uint16_t value) {
  if (value < 172) value = 172;
  if (value > 1811) value = 1811;
  return (uint16_t)(1000u + (uint32_t)(value - 172) * 1000u / 1639u);
}

void decodeSbus(const uint8_t *frame) {
  for (uint8_t i = 0; i < 16; i++) {
    const uint8_t byte = 1 + (i * 11) / 8;
    const uint8_t shift = (i * 11) % 8;
    uint32_t value = (uint32_t)frame[byte] | ((uint32_t)frame[byte + 1] << 8) |
                     ((uint32_t)frame[byte + 2] << 16);
    sbus_channels[i] = (uint16_t)((value >> shift) & 0x07FF);
  }
  const uint8_t flags = frame[23];
  sbus_failsafe = (flags & 0x08) != 0;
  sbus_lost = (flags & 0x04) != 0;
  last_sbus_ms = now_ms;
}

void sbusFeed(uint8_t byte) {
  if (sbus_len == 0 && byte != 0x0F) return;  // wait for the start byte
  sbus_buf[sbus_len++] = byte;
  if (sbus_len < sizeof(sbus_buf)) return;
  if (sbus_buf[24] == 0x00) decodeSbus(sbus_buf);
  sbus_len = 0;
}

void sendRcChannels() {
  if (!BRIDGE_SBUS_ENABLED) return;
  if (!last_sbus_ms || age(last_sbus_ms) > kSbusTimeoutMs || sbus_failsafe ||
      sbus_lost) {
    return;  // silence lets the STM32's 250 ms RC deadline expire
  }
  mavlink_message_t msg;
  mavlink_msg_rc_channels_pack(
      BRIDGE_SYSID, BRIDGE_COMPID, &msg, now_ms, 16,
      sbusToRc(sbus_channels[0]), sbusToRc(sbus_channels[1]),
      sbusToRc(sbus_channels[2]), sbusToRc(sbus_channels[3]),
      sbusToRc(sbus_channels[4]), sbusToRc(sbus_channels[5]),
      sbusToRc(sbus_channels[6]), sbusToRc(sbus_channels[7]),
      sbusToRc(sbus_channels[8]), sbusToRc(sbus_channels[9]),
      sbusToRc(sbus_channels[10]), sbusToRc(sbus_channels[11]),
      sbusToRc(sbus_channels[12]), sbusToRc(sbus_channels[13]),
      sbusToRc(sbus_channels[14]), sbusToRc(sbus_channels[15]),
      0, 0, 0);  /* CH17, CH18, RSSI */
  sendMessage(msg, false);
  sendMessage(msg, true);
}

// ---------------------------------------------------------------- link state

void serviceWiFi() {
  static uint32_t last_check_ms = 0;
  if (now_ms - last_check_ms < 5000) return;
  last_check_ms = now_ms;
  if (strlen(BRIDGE_WIFI_SSID) && WiFi.status() != WL_CONNECTED) {
    Serial.println("[bridge] WiFi disconnected, reconnecting");
    WiFi.disconnect();
    WiFi.begin(BRIDGE_WIFI_SSID, BRIDGE_WIFI_PASS);
  }
}

void setupWiFi() {
  if (strlen(BRIDGE_WIFI_SSID)) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("mecanum-bridge");
    WiFi.begin(BRIDGE_WIFI_SSID, BRIDGE_WIFI_PASS);
    Serial.printf("[bridge] joining WiFi '%s'\n", BRIDGE_WIFI_SSID);
    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
      delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[bridge] WiFi %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[bridge] WiFi join failed; check BRIDGE_WIFI_SSID");
    }
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(BRIDGE_AP_SSID, BRIDGE_AP_PASS);
    Serial.printf("[bridge] AP %s at %s\n", BRIDGE_AP_SSID,
                  WiFi.softAPIP().toString().c_str());
  }
  udp.begin(BRIDGE_UDP_PORT);
  Serial.printf("[bridge] UDP %u\n", (unsigned)BRIDGE_UDP_PORT);
}

void serviceLinks() {
  if (now_ms - last_heartbeat_ms >= kHeartbeatMs) {
    last_heartbeat_ms = now_ms;
    sendHeartbeat();
  }
  if (fc_alive() && now_ms - last_ping_ms >= kPingMs) {
    last_ping_ms = now_ms;
    sendPing();
  }
  if (now_ms - last_rc_ms >= kRcPeriodMs) {
    last_rc_ms = now_ms;
    sendRcChannels();
  }
  if (!fc_alive() && now_ms - last_status_ms >= 5000) {
    last_status_ms = now_ms;
    announce("STM32 MAVLink link lost");
  }
  digitalWrite(BRIDGE_LED, fc_alive() ? HIGH : (now_ms / 100) % 2);
}

void printStatus() {
  static uint32_t last_print_ms = 0;
  if (now_ms - last_print_ms < 2000) return;
  last_print_ms = now_ms;
  Serial.printf(
      "[bridge] fc=%s gcs=%s rtt=%.1fms sbus=%s wifi=%s ip=%s\n",
      fc_alive() ? "up" : "DOWN", gcs_alive() ? "up" : "-", ping_rtt_ms,
      (last_sbus_ms && age(last_sbus_ms) < kSbusTimeoutMs) ? "up" : "-",
      WiFi.status() == WL_CONNECTED ? "sta" : (WiFi.getMode() == WIFI_AP ? "ap" : "down"),
      WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str()
                                    : WiFi.softAPIP().toString().c_str());
}

}  // namespace

void setup() {
  pinMode(BRIDGE_LED, OUTPUT);
  digitalWrite(BRIDGE_LED, LOW);
  Serial.begin(115200);
  delay(200);

  fcPort.begin(BRIDGE_UART_BAUD, SERIAL_8N1, BRIDGE_UART_RX, BRIDGE_UART_TX);
  Serial.println("[bridge] MecanumB ESP32 radio bridge");
  Serial.printf("[bridge] UART2 rx=%d tx=%d @ %u\n", BRIDGE_UART_RX,
                BRIDGE_UART_TX, (unsigned)BRIDGE_UART_BAUD);

#if BRIDGE_SBUS_ENABLED
  Serial1.begin(100000, SERIAL_8E2, BRIDGE_SBUS_RX, -1, true);
  Serial.printf("[bridge] SBUS rx=%d inverted 8E2\n", BRIDGE_SBUS_RX);
#endif

  setupWiFi();
  last_heartbeat_ms = millis();
  last_rc_ms = millis();
}

void loop() {
  now_ms = millis();

  mavlink_message_t msg;
  mavlink_status_t status;
  while (fcPort.available()) {
    const uint8_t byte = (uint8_t)fcPort.read();
    if (mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &status)) {
      handleFcMessage(msg);
    }
  }

#if BRIDGE_SBUS_ENABLED
  while (Serial1.available()) {
    sbusFeed((uint8_t)Serial1.read());
  }
#endif

  const int packet_size = udp.parsePacket();
  if (packet_size > 0) {
    uint8_t buffer[512];
    const int length = udp.read(buffer, sizeof(buffer));
    if (length > 0) {
      registerPeer(udp.remoteIP(), udp.remotePort());
      last_gcs_rx_ms = now_ms;
      for (int i = 0; i < length; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_1, buffer[i], &msg, &status)) {
          handleGcsMessage(msg);
        }
      }
    }
  }

  serviceWiFi();
  serviceLinks();
  printStatus();
  delay(1);
}

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HardwareSerial.h>
#include "get_mavlink_message.h"

#include <sbus.h>
#include "GamePad.h"

extern "C" {
#include "common/mavlink.h"
}



/* SBUS object, reading SBUS */
bfs::SbusRx sbus_rx(&Serial2, 16, 17, 1);
/* SBUS object, writing SBUS */
bfs::SbusTx sbus_tx(&Serial2, 16, 17, 1);
/* SBUS data */
bfs::SbusData data;

// ====== MAVLink2 Packet Buffer Array ======
#define MAX_RECEIVED_PACKETS 50
uint8_t receivedPackets[MAX_RECEIVED_PACKETS][MAVLINK_MAX_PACKET_LEN];
uint16_t receivedPacketLengths[MAX_RECEIVED_PACKETS];
uint8_t packetCount_rx = 0;
uint8_t packetCount_tx = 0;

// === Global Variables ===
const char *ssid = "SECL RnD LAB";   // New WiFi SSID
const char *password = "SECL@2024";  // New WiFi Password

// const char *ssid = "Hotspot";
// const char *password = "88888888";

WiFiUDP Udp;
HardwareSerial GCS_Serial(2);  // UART2 (same as Air Unit)

// Set these uniquely per ESP32!
#define MY_STATIC_IP      IPAddress(192,168,1,101)   // <-- Change for ESP32-B!
#define GATEWAY_IP        IPAddress(192,168,1,1)
#define SUBNET_MASK       IPAddress(255,255,255,0)

#define PEER_IP           IPAddress(192,168,1,102)   // <-- Change for ESP32-B!
const int local_port = 5005;
const int peer_port  = 5005;

// ====== MAVLink Parser State ======
mavlink_message_t msg;
mavlink_status_t status;

// Buffers
uint8_t uartReceiveBuffer[512];
uint16_t uartReceiveLength = 0;

uint8_t udpReceiveBuffer[512];
uint16_t udpReceiveLength = 0;

// FreeRTOS Task Handles
TaskHandle_t txTaskHandle = NULL;
TaskHandle_t rxTaskHandle = NULL;
TaskHandle_t wifiMonitorTaskHandle = NULL;
TaskHandle_t heartbeatTaskHandle = NULL;

void sendUDPData(uint8_t *buffer, uint16_t length);

void heartbeatTask(void *param) {
  mavlink_message_t hb_msg;
  uint8_t hb_buffer[MAVLINK_MAX_PACKET_LEN];

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      // Pack a heartbeat message
      mavlink_msg_heartbeat_pack(
        1,    // system id
        200,  // component id
        &hb_msg,
        MAV_TYPE_GCS,           // Ground station type
        MAV_AUTOPILOT_INVALID,  // Not an autopilot
        MAV_MODE_MANUAL_ARMED,  // Example mode
        0,                      // Custom mode
        MAV_STATE_ACTIVE);      // System active

      uint16_t length = mavlink_msg_to_send_buffer(hb_buffer, &hb_msg);

      Udp.beginPacket(PEER_IP, peer_port);
      Udp.write(hb_buffer, length);
      Udp.endPacket();

      Serial.printf("[MAVLINK] Transmited to   UDP HEARTBEAT (ID: 0) \n");
    }

    vTaskDelay(1000 / portTICK_PERIOD_MS);  // <-- every 1000 ms = 1 second
  }
}

// ====== Send MAVLink Data to GCS via UART ======
void transmitMAVLinkData() {
  if (udpReceiveLength > 0) {
    // Parse each byte to find complete MAVLink messages
    for (int i = 0; i < udpReceiveLength; i++) {
      if (mavlink_parse_char(MAVLINK_COMM_0, udpReceiveBuffer[i], &msg, &status)) {
        // Full MAVLink message received
        Serial.printf("[MAVLINK] Received   from UDP %s (ID: %d) \n", get_mavlink_message_name(msg.msgid), msg.msgid);
      }
    }

    // Forward entire raw packet to GCS via UART
    GCS_Serial.write(udpReceiveBuffer, udpReceiveLength);
    // Serial.printf("[UART] Forwarded %d bytes to GCS over UART\n", udpReceiveLength);

    udpReceiveLength = 0;  // Reset after sending
  }
}

// ====== Receive MAVLink from GCS via UART ======
void receiveMAVLinkData() {
  static uint8_t packetBuffer[MAVLINK_MAX_PACKET_LEN];
  static uint16_t packetLength = 0;

  static uint8_t udpBuffer[MAVLINK_MAX_PACKET_LEN];
  static uint16_t udpLength = 0;

  // populate packetBuffer using your own mavlink message -----------------------------------------------------------------
  // check if buffer is non empty then send -------------------------------------------------------------------------------
  sendUDPData(packetBuffer, packetLength);


  // Safety: avoid overflow
  if (packetLength >= MAVLINK_MAX_PACKET_LEN) {
    Serial.println("[WARN] MAVLink buffer overflow, resetting...");
    packetLength = 0;
  }
  }


// ====== Send MAVLink Telemetry Over UDP ======
void sendUDPData(uint8_t *buffer, uint16_t length) {
  int beginResult =  Udp.beginPacket(PEER_IP, peer_port);
  if (!beginResult) {
    Serial.println("[ERROR] Failed to begin UDP packet");
    return;
  }

  int written = Udp.write(buffer, length);
  if (written != length) {
    Serial.printf("[ERROR] UDP write failed (%d of %d bytes written)\n", written, length);
  }

  if (!Udp.endPacket()) {
    Serial.println("[ERROR] Failed to send UDP packet");
  } else {
    // Serial.printf("[UDP] Sent %d bytes to VPS server\n", length);
  }
}

// ====== Receive UDP Data and forward to UART ======
void receiveUDPData() {
  udpReceiveLength = 0;
  int packetSize = Udp.parsePacket();
  if (packetSize > 0 && packetSize <= MAVLINK_MAX_PACKET_LEN) {
    udpReceiveLength = Udp.read(udpReceiveBuffer, MAVLINK_MAX_PACKET_LEN);
    if (udpReceiveLength > 0) {
      Serial.printf("[UDP] Received %d bytes from espA\n", udpReceiveLength);

      // === MAVLink decode logic ===
      mavlink_message_t rx_msg;
      mavlink_status_t rx_status;
      for (int i = 0; i < udpReceiveLength; i++) {
        if (mavlink_parse_char(MAVLINK_COMM_0, udpReceiveBuffer[i], &rx_msg, &rx_status)) {
          // Successfully decoded a MAVLink message
          Serial.printf("[MAVLINK] Decoded message: %s (ID: %d)\n", get_mavlink_message_name(rx_msg.msgid), rx_msg.msgid);

          // Example: If you want to handle RC_CHANNELS_OVERRIDE
          if (rx_msg.msgid == MAVLINK_MSG_ID_RC_CHANNELS_OVERRIDE) {
            mavlink_rc_channels_override_t rc_override;
            mavlink_msg_rc_channels_override_decode(&rx_msg, &rc_override);
            Serial.print("RC Override channels: ");
            Serial.print(rc_override.chan1_raw); Serial.print(" ");
            Serial.print(rc_override.chan2_raw); Serial.print(" ");
            Serial.print(rc_override.chan3_raw); Serial.print(" ");
            Serial.print(rc_override.chan4_raw); Serial.print(" ");
            Serial.print(rc_override.chan5_raw); Serial.print(" ");
            Serial.print(rc_override.chan6_raw); Serial.print(" ");
            Serial.print(rc_override.chan7_raw); Serial.print(" ");
            Serial.print(rc_override.chan8_raw); Serial.println();
          }

          // You can add more message handling here if needed
        }
      }
      // === End MAVLink decode logic ===

    } else {
      Serial.println("[ERROR] UDP read failed");
    }
  }
}

// ====== Transmit Task ======
void UdpToMavTask(void *param) {
  while (true) {
    receiveUDPData();                    // Check if any packet available
    vTaskDelay(2 / portTICK_PERIOD_MS);  // Short delay to yield CPU
  }
}

// ====== Receive Task ======
void MavToUdpTask(void *param) {
  while (true) {
    receiveMAVLinkData();  // Read any incoming UART bytes
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

// ====== WiFi Monitoring Task ======
void wifiMonitorTask(void *param) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("[WIFI] Disconnected. Attempting reconnection...");
      WiFi.disconnect(true);
      WiFi.begin(ssid, password);

      unsigned long retryStart = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - retryStart < 10000) {
        delay(500);
        Serial.print(".");
      }

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WIFI] Reconnected successfully.");
        Serial.print("[WIFI] IP Address: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println("\n[WIFI] Reconnection failed.");
      }
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
  const uint8_t *addr = BP32.localBdAddress();
  Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.forgetBluetoothKeys();
  BP32.enableVirtualDevice(false);


  Serial.println("[BOOT] Ground Station Starting...");

  // --- Set Static IP ---
  WiFi.config(MY_STATIC_IP, GATEWAY_IP, SUBNET_MASK);

  WiFi.begin(ssid, password);
  Serial.print("[INFO] Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n[INFO] WiFi connected");
  Serial.print("[INFO] IP Address: ");
  Serial.println(WiFi.localIP());

  if (Udp.begin(local_port)) {
    Serial.printf("[INFO] UDP listener started on port %d\n", local_port);
  } else {
    Serial.println("[ERROR] Failed to start UDP listener");
  }

 // Optional: Hello packet to peer
  Udp.beginPacket(PEER_IP, peer_port);
  Udp.print("HELLO");
  Udp.endPacket();

  // Tasks
  // 🧠 Heavy Tasks (require more stack + higher priority)
  xTaskCreatePinnedToCore(UdpToMavTask,  // esp B
                          "TX Task",     // Task name
                          4096,          // Stack size in bytes (8 KB for heavy parsing/forwarding)
                          NULL,
                          2,  // Priority (high)
                          &txTaskHandle,
                          1);  // Core 1

  xTaskCreatePinnedToCore(MavToUdpTask,  // esp A
                          "RX Task",
                          4096,
                          NULL,
                          2,  // High priority
                          &rxTaskHandle,
                          1);  // Core 1

  // 🌐 Lightweight Task (Wi-Fi reconnect handler)
  xTaskCreatePinnedToCore(wifiMonitorTask,
                          "WiFi Monitor",
                          3072,  // Small stack (2 KB is enough)
                          NULL,
                          1,  // Low priority
                          &wifiMonitorTaskHandle,
                          1);  // Core 0

  // ❤️ Lightweight Task (heartbeat sender)
  xTaskCreatePinnedToCore(heartbeatTask,
                          "Heartbeat Task",
                          3072, // Small stack (3 KB)
                          NULL,
                          0, // Low priority
                          &heartbeatTaskHandle,
                          0); // Core 0

  Serial.println("[INFO] Tasks started");

  // initialize sbus serial 
  sbus_rx.Begin();
  sbus_tx.Begin();
}

// ====== Loop ======
void loop() {
  // All functionality handled by FreeRTOS tasks
  bool dataUpdated = BP32.update();
  if (dataUpdated) {
    processControllers();
  }

  // === MAVLink RC_CHANNELS_OVERRIDE logic: send to both UDP and Serial2 ===
  mavlink_message_t msg;
  uint8_t packetBuffer[MAVLINK_MAX_PACKET_LEN];
  uint16_t packetLength = 0;

  mavlink_msg_rc_channels_override_pack(
      1, 200, &msg, 1, 1,
      channel[0], channel[1], channel[2], channel[3],
      channel[4], channel[5], channel[6], channel[7],
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  );

  packetLength = mavlink_msg_to_send_buffer(packetBuffer, &msg);

  // Send over Serial2
  Serial2.write(packetBuffer, packetLength);

  if (packetLength > 0) {
    // Send over UDP
    sendUDPData(packetBuffer, packetLength);
  }

  // Debug print
  Serial.print("CH0: "); Serial.print(channel[0]);
  Serial.print(" CH1: "); Serial.print(channel[1]);
  Serial.print(" CH2: "); Serial.print(channel[2]);
  Serial.print(" CH3: "); Serial.print(channel[3]);
  Serial.print(" CH4: "); Serial.print(channel[4]);
  Serial.println();
  // === End MAVLink RC_CHANNELS_OVERRIDE logic ===

  /* Display the channel data */
  Serial.println("channel data");
  for (int8_t i = 0; i < data.NUM_CH; i++) {
    data.ch[i] = channel[i];
    Serial.print(" channel["); Serial.print(i); Serial.print("] = ");
    Serial.print(channel[i]);
  }
  /* Set the SBUS TX data */
  sbus_tx.data(data);
  /* Write the data to the servos */
  sbus_tx.Write();
  vTaskDelay(20);
}

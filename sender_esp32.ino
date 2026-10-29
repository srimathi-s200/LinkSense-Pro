
#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "LinkSense_Network";
const char* password = "LinkSense123";

const IPAddress receiverIP(192, 168, 4, 2);
const unsigned int localPort = 4210;
const unsigned int receiverPort = 4210;

const unsigned long ACK_TIMEOUT_MS = 500;
const unsigned long TEST_WINDOW_MS = 60000;
const int MAX_RETRIES = 3;

WiFiUDP udp;

uint32_t sequenceNumber = 0;

uint32_t uniquePackets = 0;
uint32_t ackedPackets = 0;
uint32_t failedPackets = 0;
uint32_t totalTransmissions = 0;
uint32_t totalRetries = 0;
uint64_t totalRttMs = 0;

unsigned long windowStart = 0;

uint16_t crc16(const char* data) {
  uint16_t crc = 0xFFFF;

  while (*data) {
    crc ^= ((uint16_t)(uint8_t)*data++) << 8;

    for (int i = 0; i < 8; i++) {
      crc = (crc & 0x8000)
          ? (crc << 1) ^ 0x1021
          : crc << 1;
    }
  }

  return crc;
}

bool waitForAck(uint32_t expectedSeq, unsigned long& rtt) {
  unsigned long start = millis();

  while (millis() - start < ACK_TIMEOUT_MS) {
    int size = udp.parsePacket();

    if (size > 0) {
      char reply[256];
      int len = udp.read(reply, sizeof(reply) - 1);

      if (len <= 0) continue;
      reply[len] = '\0';

      unsigned long parsedSeq = 0;

      if (sscanf(reply, "ACK,SEQ=%lu", &parsedSeq) == 1 &&
          parsedSeq == expectedSeq) {
        rtt = millis() - start;
        return true;
      }
    }

    delay(2);
  }

  return false;
}

void printAndResetWindow() {
  Serial.println("\n===== 60-SECOND REPORT =====");

  Serial.print("Unique packets: ");
  Serial.println(uniquePackets);

  Serial.print("ACKed packets: ");
  Serial.println(ackedPackets);

  Serial.print("Failed packets: ");
  Serial.println(failedPackets);

  Serial.print("Total transmissions: ");
  Serial.println(totalTransmissions);

  Serial.print("Retries: ");
  Serial.println(totalRetries);

  if (uniquePackets > 0) {
    float pdr = 100.0f * ackedPackets / uniquePackets;
    Serial.print("PDR (%): ");
    Serial.println(pdr, 2);
  }

  if (totalTransmissions > 0) {
    float overhead = 100.0f * totalRetries / totalTransmissions;
    Serial.print("Retry overhead (% of transmissions): ");
    Serial.println(overhead, 2);
  }

  if (ackedPackets > 0) {
    float averageRtt = (float)totalRttMs / ackedPackets;
    Serial.print("Average RTT (ms): ");
    Serial.println(averageRtt, 2);
  }

  Serial.println("============================\n");

  uniquePackets = 0;
  ackedPackets = 0;
  failedPackets = 0;
  totalTransmissions = 0;
  totalRetries = 0;
  totalRttMs = 0;
  windowStart = millis();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP(ssid, password)) {
    Serial.println("ERROR: AP startup failed");
    while (true) delay(1000);
  }

  Serial.print("ESP32 AP IP: ");
  Serial.println(WiFi.softAPIP());

  if (!udp.begin(localPort)) {
    Serial.println("ERROR: UDP startup failed");
    while (true) delay(1000);
  }

  windowStart = millis();
  Serial.println("LinkSense Pro - Stage 4A Analytics");
}

void loop() {
  if (millis() - windowStart >= TEST_WINDOW_MS) {
    printAndResetWindow();
  }

  sequenceNumber++;
  uniquePackets++;

  char body[80];
  snprintf(body, sizeof(body), "LS1,%lu,HELLO",
           (unsigned long)sequenceNumber);

  uint16_t crc = crc16(body);
  char packet[100];

  snprintf(packet, sizeof(packet), "%s,%04X", body, crc);

  bool success = false;
  unsigned long rtt = 0;

  for (int attempt = 0; attempt <= MAX_RETRIES; attempt++) {
    // Discard replies left over from earlier attempts.
    while (udp.parsePacket() > 0) {
      char discard[256];
      udp.read(discard, sizeof(discard));
    }

    udp.beginPacket(receiverIP, receiverPort);
    udp.write((const uint8_t*)packet, strlen(packet));
    int sent = udp.endPacket();

    totalTransmissions++;

    if (attempt > 0) {
      totalRetries++;
    }

    if (sent && waitForAck(sequenceNumber, rtt)) {
      success = true;
      ackedPackets++;
      totalRttMs += rtt;
      break;
    }
  }

  if (!success) {
    failedPackets++;
  }

  // Stop-and-wait pacing: one packet per cycle.
  delay(100);
}


#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "LinkSense_Network";
const char* password = "LinkSense123";

const unsigned int localPort = 4210;
const IPAddress senderIP(192, 168, 4, 1);
const unsigned int senderPort = 4210;

WiFiUDP udp;

uint32_t lastSequence = 0;
uint32_t validPackets = 0;
uint32_t missingPackets = 0;
uint32_t duplicates = 0;
uint32_t outOfOrder = 0;
uint32_t crcFailures = 0;

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

void sendAck(uint32_t seq) {
  char reply[100];

  snprintf(reply, sizeof(reply),
           "ACK,SEQ=%lu,VALID=%lu,MISSING=%lu,CRCFAIL=%lu",
           (unsigned long)seq,
           (unsigned long)validPackets,
           (unsigned long)missingPackets,
           (unsigned long)crcFailures);

  udp.beginPacket(senderIP, senderPort);
  udp.print(reply);
  udp.endPacket();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to ESP32");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi connected");
  Serial.print("NodeMCU IP: ");
  Serial.println(WiFi.localIP());

  if (!udp.begin(localPort)) {
    Serial.println("ERROR: UDP startup failed");
    while (true) delay(1000);
  }

  Serial.println("LinkSense Pro - Reliable Receiver");
}

void loop() {
  int packetSize = udp.parsePacket();

  if (packetSize <= 0) {
    delay(2);
    return;
  }

  char packet[256];
  int len = udp.read(packet, sizeof(packet) - 1);

  if (len <= 0) return;
  packet[len] = '\0';

  char* crcField = strrchr(packet, ',');

  if (crcField == nullptr) {
    Serial.println("INVALID: missing CRC");
    return;
  }

  *crcField = '\0';
  char* crcText = crcField + 1;

  if (strlen(crcText) != 4) {
    Serial.println("INVALID: CRC format");
    return;
  }

  char* crcEnd;
  unsigned long receivedCRC = strtoul(crcText, &crcEnd, 16);

  if (*crcText == '\0' || *crcEnd != '\0' ||
      receivedCRC > 0xFFFF) {
    Serial.println("INVALID: CRC value");
    return;
  }

  uint16_t calculatedCRC = crc16(packet);

  if (calculatedCRC != (uint16_t)receivedCRC) {
    crcFailures++;
    Serial.println("CRC FAIL");
    return;
  }

  char* savePtr;
  char* version = strtok_r(packet, ",", &savePtr);
  char* seqText = strtok_r(nullptr, ",", &savePtr);
  char* payload = strtok_r(nullptr, ",", &savePtr);

  if (version == nullptr || seqText == nullptr ||
      payload == nullptr || strcmp(version, "LS1") != 0) {
    Serial.println("INVALID: packet format");
    return;
  }

  char* seqEnd;
  unsigned long seq = strtoul(seqText, &seqEnd, 10);

  if (*seqText == '\0' || *seqEnd != '\0' ||
      seq == 0 || seq > 0xFFFFFFFFUL) {
    Serial.println("INVALID: sequence");
    return;
  }

  if (seq == lastSequence) {
    duplicates++;
    Serial.print("DUPLICATE: Seq ");
    Serial.println(seq);
    sendAck(seq);
    return;
  }

  if (seq < lastSequence) {
    outOfOrder++;
    Serial.print("OUT OF ORDER: Seq ");
    Serial.println(seq);
    sendAck(seq);
    return;
  }

  if (lastSequence != 0 && seq > lastSequence + 1) {
    uint32_t gap = seq - lastSequence - 1;
    missingPackets += gap;

    Serial.print("Missing sequence count: ");
    Serial.println(gap);
  }

  lastSequence = seq;
  validPackets++;

  Serial.print("VALID | Seq: ");
  Serial.print(seq);
  Serial.print(" | Payload: ");
  Serial.println(payload);

  sendAck(seq);

  Serial.print("Valid=");
  Serial.print(validPackets);
  Serial.print(" Missing=");
  Serial.print(missingPackets);
  Serial.print(" Duplicates=");
  Serial.print(duplicates);
  Serial.print(" OutOfOrder=");
  Serial.print(outOfOrder);
  Serial.print(" CRCFailures=");
  Serial.println(crcFailures);
}

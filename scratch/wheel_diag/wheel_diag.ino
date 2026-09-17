// wheel_diag.ino
// Throwaway diagnostic sketch — NOT part of core/firmware. Uses hardware
// interrupts (not polling) on both of the scroll wheel sensor's non-GND
// pins, so we can't miss a transition no matter how brief it is — settles
// definitively whether "middle" (previously assumed to be VCC) ever
// transitions at all, and if so, its timing relative to "bottom".

#include <Adafruit_TinyUSB.h>  // required for Serial to link on this core.

const int CHANNEL_BOTTOM_PIN = A4;  // P0.02, labeled "002"
const int CHANNEL_MIDDLE_PIN = A7;  // P0.31, labeled "031"

const size_t BUF_SIZE = 2048;
volatile unsigned long edgeTime[BUF_SIZE];
volatile uint8_t edgeChannel[BUF_SIZE];  // 0 = bottom, 1 = middle
volatile uint8_t edgeLevel[BUF_SIZE];    // resulting digital level after the edge
volatile size_t head = 0;
volatile size_t tail = 0;

// Counted unconditionally, independent of the detail buffer above — these
// can never miss an edge even if the detail buffer fills up and old entries
// get dropped, so they're the authoritative answer to "did this pin ever
// transition at all."
volatile unsigned long bottomEdgeCount = 0;
volatile unsigned long middleEdgeCount = 0;

void recordEdge(uint8_t channel, uint8_t level) {
  size_t next = (head + 1) % BUF_SIZE;
  if (next != tail) {  // drop detail silently if the buffer's full; counts
                        // below still stay accurate regardless
    edgeTime[head] = micros();
    edgeChannel[head] = channel;
    edgeLevel[head] = level;
    head = next;
  }
  if (channel == 0) {
    bottomEdgeCount++;
  } else {
    middleEdgeCount++;
  }
}

void bottomISR() { recordEdge(0, digitalRead(CHANNEL_BOTTOM_PIN)); }
void middleISR() { recordEdge(1, digitalRead(CHANNEL_MIDDLE_PIN)); }

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  pinMode(CHANNEL_BOTTOM_PIN, INPUT);
  pinMode(CHANNEL_MIDDLE_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(CHANNEL_BOTTOM_PIN), bottomISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(CHANNEL_MIDDLE_PIN), middleISR, CHANGE);

  Serial.println("micros,channel,level");
}

void loop() {
  while (tail != head) {
    unsigned long t = edgeTime[tail];
    uint8_t ch = edgeChannel[tail];
    uint8_t lvl = edgeLevel[tail];
    tail = (tail + 1) % BUF_SIZE;

    Serial.print(t);
    Serial.print(',');
    Serial.print(ch == 0 ? "bottom" : "middle");
    Serial.print(',');
    Serial.println(lvl);
  }

  // Periodic total-edge-count summary (prefixed "#" so it's easy to filter
  // out when parsing the CSV rows above) — the real answer to "did middle
  // ever move at all," immune to any buffer/print backlog.
  static unsigned long lastSummary = 0;
  unsigned long now = millis();
  if (now - lastSummary >= 2000) {
    lastSummary = now;
    noInterrupts();
    unsigned long bc = bottomEdgeCount;
    unsigned long mc = middleEdgeCount;
    interrupts();
    Serial.print("# summary: bottom_edges=");
    Serial.print(bc);
    Serial.print(" middle_edges=");
    Serial.println(mc);
  }
}

#include <RTTStream.h>
#include <SWOStream.h>
#include <STM32_CAN.h>

//#define LED LED_BUILTIN
#define LED PB2 // WeAct STM32F412 CoreBoard

RTTStream rtt;
SWOStream swo(1000000);

//STM32_CAN Can1(CAN1, DEF);  // CAN1, default pins PA11=RX / PA12=TX
STM32_CAN Can1(CAN1, ALT);  // CAN1, alt pins PB8=RX / PB9=TX

uint32_t count = 0;
uint32_t led_on = HIGH;

void setup() {
  pinMode(LED, OUTPUT);
  Serial1.begin(115200);

  Can1.setBaudRate(500000);
  Can1.setAutoBusOffRecovery(true);
  Can1.begin();
}

void can_send_counter(uint32_t value) {
  CAN_message_t msg;
  msg.id = 0x100 | value & 0x0F;
  msg.len = 4;
  msg.buf[0] = (value >> 24) & 0xFF;
  msg.buf[1] = (value >> 16) & 0xFF;
  msg.buf[2] = (value >> 8) & 0xFF;
  msg.buf[3] = (value >> 0) & 0xFF;
  Can1.write(msg);
}

void loop() {
  count++;

  Serial1.print("serial: ");
  Serial1.println(count);

  if (Serial1.available()) {
    Serial1.print("serial: >");
    while (Serial1.available())
      Serial1.print((char)Serial1.read());
    Serial1.println();
  }

  rtt.print("rtt: ");
  rtt.println(count);

  swo.print("swo: ");
  swo.println(count);

  can_send_counter(count);

  led_on = 1 - led_on;
  digitalWrite(LED, led_on);
  delay(1000);
}

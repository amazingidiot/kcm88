#include <Arduino.h>
#include <QNEthernet.h>

namespace qn = qindesign::network;

qn::EthernetUDP server;

IPAddress ip (192,168,100,10);
IPAddress subnet (255,255,255,0);


void setup() {
  if (! qn::Ethernet.begin(ip, subnet, INADDR_NONE)) {return;}

  
}

void loop() {
  // put your main code here, to run repeatedly:
}

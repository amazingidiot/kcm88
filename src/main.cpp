#include <Arduino.h>
#include <QNEthernet.h>

namespace qn = qindesign::network;

qn::EthernetUDP server;

uint8_t buf[qn::Ethernet.mtu() - 20 - 8];

// Local IP settings
IPAddress local_ip(192, 168, 100, 10);
IPAddress local_subnet(255, 255, 255, 0);
static uint16_t local_port = 8000;

// Remote IP settings
IPAddress remote_ip(192, 168, 100, 1);
static uint16_t remote_port = 8000;

void setup()
{
    if (!qn::Ethernet.begin(local_ip, local_subnet, INADDR_NONE)) {
        return;
    }

    server.begin(local_port);
}

void loop()
{
    int size = server.parsePacket();

    if (0 < size && static_cast<unsigned int>(size) <= sizeof(buf)) {
        server.read(buf, size);

	server.send(remote_ip, remote_port, buf, size);
    }
}

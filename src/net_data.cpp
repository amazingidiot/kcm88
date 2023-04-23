#include "net_data.h"

#include <LiteOSCParser.h>
#include <QNEthernet.h>

namespace net = qindesign::network;
namespace osc = qindesign::osc;

// Buffer for received UDP messages
static uint8_t buf[net::Ethernet.mtu() - 20 - 8];
static const uint8_t queueSize = 8;

static osc::LiteOSCParser parser;
static osc::LiteOSCParser log_parser;
static osc::OSCBundle bundle;

net::EthernetUDP server(queueSize);

// Local IP settings
static IPAddress local_ip(192, 168, 100, 10);
static IPAddress local_subnet(255, 255, 255, 0);
static uint16_t local_port = 8000;

// Remote IP settings
static IPAddress remote_ip(192, 168, 100, 1);
static uint16_t remote_port = 8000;

void setupNetData()
{
    if (!net::Ethernet.begin(local_ip, local_subnet, INADDR_NONE)) {
        return;
    }

    server.begin(local_port);
}

void loopNetData()
{
    int size = server.parsePacket();

    if (0 < size && static_cast<unsigned int>(size) <= sizeof(buf)) {
        server.read(buf, size);
    }
}

void sendData()
{
    server.send(remote_ip, remote_port, parser.getMessageBuf(), parser.getMessageSize());
    server.flush();
}

void sendNoteOn(uint8_t note, float velocity)
{
    parser.init("/noteon");
    parser.addInt(note);
    parser.addInt((int32_t)(126.0f * velocity) + 1);

    sendData();
}

void sendNoteOff(uint8_t note, float velocity)
{
    parser.init("/noteoff");
    parser.addInt(note);
    parser.addInt((int32_t)(126.0f * velocity) + 1);

    sendData();
}

void sendPedalDamper(bool pressed)
{
    parser.init("/pedal/damper");
    parser.addBoolean(pressed);

    sendData();
}

void sendPedal(float value)
{
    parser.init("/pedal/expression");

    parser.addFloat(value);

    sendData();
}

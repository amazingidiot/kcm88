#include "net.h"
#include "config.h"
#include <LiteOSCParser.h>
#include <QNEthernet.h>
#include <algorithm>
#include <vector>

namespace net = qindesign::network;
namespace osc = qindesign::osc;

static osc::LiteOSCParser parser;

net::EthernetServer server;

struct ClientState {
    ClientState(net::EthernetClient client)
        : client(std::move(client))
    {
        reset();
    }

    net::EthernetClient client;
    bool closed = false;

    // Reset the client state.
    void reset()
    {
    }
};

std::vector<ClientState> clients;

int setupNet()
{
    net::Ethernet.onLinkState([](bool state) {
        if (!state) {
            // No link is available
            return;
        }

        // TODO Handle new link here
    });
    net::Ethernet.onAddressChanged([]() {
        IPAddress address = net::Ethernet.localIP();

        if (address == INADDR_NONE) {
            // No IP address available
            server.end();

            clients.clear();

            return;
        }

        server.begin(NET_PORT);

        // TODO Handle ip address change here. Can happen when DHCP server assigns a new address
    });

    // Start Ethernet with DHCP
    if (!net::Ethernet.begin()) {
        // Could not start Ethernet
        return -1;
    }

    net::Ethernet.setHostname(NET_HOSTNAME);

    if (!net::MDNS.begin(NET_HOSTNAME)) {
        // Could not start MDNS service
        return -1;
    }

    net::MDNS.addService(NET_SERVICENAME, "_osc", "_tcp", 8000);

    return 0;
}

void loopNet()
{
    net::EthernetClient client = server.accept();

    if (client) {
        client.setNoDelay(true);
        clients.emplace_back(std::move(client));
    }
}

void sendData()
{
    for (auto client : clients) {
        client.client.writeFully(parser.getMessageBuf(), parser.getMessageSize());

        client.client.flush();
    }
}

void sendNoteOn(uint8_t note, float velocity)
{
    parser.init("/noteon");
    parser.addInt(note);
    parser.addFloat(velocity);

    sendData();
}

void sendNoteOff(uint8_t note, float velocity)
{
    parser.init("/noteoff");
    parser.addInt(note);
    parser.addFloat(velocity);

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

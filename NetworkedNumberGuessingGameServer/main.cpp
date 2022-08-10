#include <enet/enet.h>
#include <iostream>
#include <chrono>
#include <map>

using namespace std;

ENetAddress address;
ENetHost* server;

const int fakeMessageTime = 5;
int lastFakeMessage = 0;

const int fakeMessagesSize = 5;
string fakeMessages[fakeMessagesSize] = { "Just who do you think you are?",
                                        "It's time for you to leave.",
                                        "ITS OVER 9000",
                                        "Do you really think you should have said that?",
                                        "How about no?" };

map<ENetPeer*, string> peerToNameMap;

const char messageTypeMessage = 'm';
const char messageTypeConnect = 'c';

bool CreateServer()
{
    cout << "Creating server..." << endl;

    /* Bind the server to the default localhost.     */
    /* A specific host address can be specified by   */
    /* enet_address_set_host (& address, "x.x.x.x"); */
    address.host = ENET_HOST_ANY;
    /* Bind the server to port 1234. */
    address.port = 1234;
    server = enet_host_create(&address /* the address to bind the server host to */,
        32      /* allow up to 32 clients and/or outgoing connections */,
        2      /* allow up to 2 channels to be used, 0 and 1 */,
        0      /* assume any amount of incoming bandwidth */,
        0      /* assume any amount of outgoing bandwidth */);

    return server != NULL;
}

string GetUserNameFromPeer(ENetPeer* peer)
{
    auto iterator = peerToNameMap.find(peer);

    if (iterator != peerToNameMap.end())
    {
        return peerToNameMap.at(peer);
    }

    return nullptr;
}

uint32_t GetTime()
{
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

void SendMessage(string message)
{
    /* Create a reliable packet of size 7 containing "packet\0" */
    ENetPacket* packet = enet_packet_create(message.c_str(),
        strlen(message.c_str()) + 1,
        ENET_PACKET_FLAG_RELIABLE);

    /* Send the packet to the peer over channel id 0. */
    /* One could also broadcast the packet by         */
    enet_host_broadcast(server, 0, packet);

    /* One could just use enet_host_service() instead. */
    enet_host_flush(server);
}

int GetNumberOfConnections()
{
    int total = 0;

    for (int i = 0; i < server->peerCount; i++)
    {
        ENetPeer tempPeer = server->peers[i];
        if (tempPeer.state == ENET_PEER_STATE_CONNECTED) total++;
    }

    return total;
}

void CheckIfShouldSendFakeMessage()
{
    int numberOfConnections = GetNumberOfConnections();

    int currentTime = GetTime();
    if (numberOfConnections > 1 && currentTime > lastFakeMessage + fakeMessageTime)
    {
        int randomUserNameIndex = rand() % numberOfConnections;

        string userName = "";

        int i = 0;

        for (auto iterator = peerToNameMap.begin(); iterator != peerToNameMap.end(); ++iterator)
        {
            if (i == randomUserNameIndex)
            {
                userName = iterator->second;
                break;
            }

            i++;
        }

        // send fake message
        SendMessage(userName + ": " + fakeMessages[rand() % fakeMessagesSize]);
        lastFakeMessage = currentTime;
    }
}

void HandleEventTypeReceive(ENetEvent event)
{
    string eventPacketData = (char*)event.packet->data;

    char messageType = eventPacketData[0];
    string restOfMessage = eventPacketData.substr(1);

    if (messageType == messageTypeConnect)
    {
        auto iterator = peerToNameMap.find(event.peer);

        if (iterator == peerToNameMap.end())
        {
            peerToNameMap.insert(pair<ENetPeer*, string>(event.peer, restOfMessage));

            SendMessage(restOfMessage + " has entered the chat.");
        }
    }
    else if (messageType == messageTypeMessage)
    {
        SendMessage(GetUserNameFromPeer(event.peer) + ": " + restOfMessage);
    }
}

void HandleEventTypeDisconnect(ENetEvent event)
{
    cout << "System: A peer has disconnected. Connections: "
        << GetNumberOfConnections() << endl;

    auto iterator = peerToNameMap.find(event.peer);

    if (iterator != peerToNameMap.end())
    {
        SendMessage(GetUserNameFromPeer(event.peer) + " has left the chat.");

        peerToNameMap.erase(iterator);
    }
}

int main(int argc, char** argv)
{
    /* initialize random seed: */
    srand(time(NULL));

    if (enet_initialize() != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        cout << "An error occurred while initializing ENet." << endl;
        return EXIT_FAILURE;
    }

    atexit(enet_deinitialize);

    if (!CreateServer())
    {
        fprintf(stderr,
            "An error occurred while trying to create an ENet server host.\n");
        cout << "An error occurred while trying to create an ENet server host." << endl;
        ::exit(EXIT_FAILURE);
    }

    cout << "Server created..." << endl;

    while (true)
    {
        ENetEvent event;

        /* Wait up to 1000 milliseconds for an event. */
        while (enet_host_service(server, &event, 1000) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_CONNECT:
                cout << "System: A new peer has connected. Connections: "
                    << GetNumberOfConnections() << endl;

                break;
            case ENET_EVENT_TYPE_RECEIVE:
            {
                HandleEventTypeReceive(event);

                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);

                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                HandleEventTypeDisconnect(event);

                /* Reset the peer's client information. */
                event.peer->data = NULL;
            }
        }

        CheckIfShouldSendFakeMessage();
    }

    if (server != NULL) enet_host_destroy(server);

    return EXIT_SUCCESS;
}
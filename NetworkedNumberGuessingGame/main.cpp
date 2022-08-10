#define NOMINMAX
#include <enet/enet.h>
#include <iostream>
#include <conio.h>
#include <thread>
#include <string>
#include "GamePacket.h"

using namespace std;

ENetAddress address;
ENetHost* client;
ENetPeer* peer;

string username;
bool disconnect;

bool CreateClient()
{
    cout << "Creating client..." << endl << endl;
    client = enet_host_create(NULL /* create a client host */,
        1 /* only allow 1 outgoing connection */,
        2 /* allow up 2 channels to be used, 0 and 1 */,
        0 /* assume any amount of incoming bandwidth */,
        0 /* assume any amount of outgoing bandwidth */);

    return client != NULL;
}

void HandleInvalidInput()
{
    cout << "Invalid input" << endl;
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

int GetValidInput(int max)
{
    int selection;

    bool retry;

    do
    {
        retry = false;

        cin >> selection;

        if (cin.fail() || selection < 1 || selection > max)
        {
            HandleInvalidInput();

            retry = true;
        }
    } while (retry);

    cout << endl;

    return selection;
}

void SendUserInfoGamePacket()
{
    UserInfoGamePacket userInfoGP;
    userInfoGP.username = username;

    // Create a character buffer as long as the game packet size (function defined on the struct)
    size_t dataSize = userInfoGP.size();
    char* data = new char[dataSize];

    // Pack message struct into character buffer
    UserInfoGamePacket::serialize(userInfoGP, data);

    /* Create a reliable packet of size 7 containing "packet\0" */
    ENetPacket* packet = enet_packet_create(data,
        dataSize,
        ENET_PACKET_FLAG_RELIABLE);

    /* Send the packet to the peer over channel id 0. */
    /* One could also broadcast the packet by         */
    enet_host_broadcast(client, 0, packet);

    /* One could just use enet_host_service() instead. */
    enet_host_flush(client);
}

void SendUserGuessGamePacket(int number)
{
    UserGuessGamePacket userGuessGP;
    userGuessGP.number = number;

    // Create a character buffer as long as the game packet size (function defined on the struct)
    size_t dataSize = userGuessGP.size();
    char* data = new char[dataSize];

    // Pack message struct into character buffer
    UserGuessGamePacket::serialize(userGuessGP, data);

    /* Create a reliable packet of size 7 containing "packet\0" */
    ENetPacket* packet = enet_packet_create(data,
        dataSize,
        ENET_PACKET_FLAG_RELIABLE);

    /* Send the packet to the peer over channel id 0. */
    /* One could also broadcast the packet by         */
    enet_host_broadcast(client, 0, packet);

    /* One could just use enet_host_service() instead. */
    enet_host_flush(client);
}

void HandleEventTypeReceiveGamePacket(ENetEvent event)
{
    GamePacket* gamePacket = (GamePacket*)event.packet->data;

    if (gamePacket)
    {
        if (gamePacket->type == PHT_Message)
        {
            MessageGamePacket messageGP;
            MessageGamePacket::deserialize((char*)event.packet->data, event.packet->dataLength, messageGP);

            cout << messageGP.message << endl;
        }
        else if (gamePacket->type == PHT_UserGuess)
        {
            UserGuessGamePacket userGuessGP;
            UserGuessGamePacket::deserialize((char*)event.packet->data, event.packet->dataLength, userGuessGP);

            cout << "Please enter your guess." << endl;

            int guess = GetValidInput(userGuessGP.number);

            cout << "Sending your guess." << endl;

            SendUserGuessGamePacket(guess);
        }
    }
}

void LeaveGame()
{
    ENetEvent event;
    enet_peer_disconnect(peer, 0);

    /* Allow up to 3 seconds for the disconnect to succeed
     * and drop any packets received packets.
     */
    while (enet_host_service(client, &event, 3000) > 0)
    {
        switch (event.type)
        {
        case ENET_EVENT_TYPE_RECEIVE:
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            cout << "Disconnection succeeded." << endl;
            return;
        }
    }

    /* We've arrived here, so the disconnect attempt didn't */
    /* succeed yet.  Force the connection down.             */
    enet_peer_reset(peer);
}

int main(int argc, char** argv)
{
    cout << "What is your name?" << endl;

    cin >> username;

    cout << endl;

    if (enet_initialize() != 0)
    {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        cout << "An error occurred while initializing ENet." << endl;
        return EXIT_FAILURE;
    }

    atexit(enet_deinitialize);

    if (!CreateClient())
    {
        fprintf(stderr,
            "An error occurred while trying to create an ENet client host.\n");
        exit(EXIT_FAILURE);
    }

    ENetEvent event;

    /* Connect to host */
    enet_address_set_host(&address, "127.0.0.1");
    address.port = 1234;

    /* Initiate the connection, allocating the two channels 0 and 1. */
    peer = enet_host_connect(client, &address, 2, 0);
    if (peer == NULL)
    {
        fprintf(stderr,
            "No available peers for initiating an ENet connection.\n");
        cout << "No available peers for initiating an ENet connection." << endl;
        exit(EXIT_FAILURE);
    }

    /* Wait up to 5 seconds for the connection attempt to succeed. */
    if (enet_host_service(client, &event, 5000) > 0 &&
        event.type == ENET_EVENT_TYPE_CONNECT)
    {
        cout << "Connected to the game." << endl;

        SendUserInfoGamePacket();
    }
    else
    {
        /* Either the 5 seconds are up or a disconnect event was */
        /* received. Reset the peer in the event the 5 seconds   */
        /* had run out without any significant event.            */
        enet_peer_reset(peer);
        cout << "Connection to 127.0.0.1:1234 failed." << endl;
    }

    while (!disconnect)
    {
        ENetEvent event;

        /* Wait up to 1000 milliseconds for an event. */
        while (enet_host_service(client, &event, 1000) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_RECEIVE:
                HandleEventTypeReceiveGamePacket(event);

                /* Clean up the packet now that we're done using it. */
                enet_packet_destroy(event.packet);

                break;
            }
        }
    }

    LeaveGame();

    if (client != NULL) enet_host_destroy(client);

    return EXIT_SUCCESS;
}
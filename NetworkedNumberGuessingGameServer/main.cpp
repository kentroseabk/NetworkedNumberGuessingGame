#include <enet/enet.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <map>
#include "GamePacket.h"

using namespace std;

ENetAddress address;
ENetHost* server;

map<ENetPeer*, string> peerToNameMap;

const int maxNumber = 100;
int numberToGuess = 0;

const int requiredNumberOfPlayersToBegin = 2;

bool gameStarted = false;

ENetPeer* activePeer;

bool waitingOnPeer = false;

int timeToWaitForNextGame = 5000;

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

string GetUserNameFromPeer(ENetPeer* peer)
{
    auto iterator = peerToNameMap.find(peer);

    if (iterator != peerToNameMap.end())
    {
        return peerToNameMap.at(peer);
    }

    return nullptr;
}

void BroadcastMessage(string message)
{
    MessageGamePacket messageGP;
    messageGP.message = message;

    size_t dataSize = messageGP.size();
    char* data = new char[dataSize];

    MessageGamePacket::serialize(messageGP, data);

    /* Create a reliable packet of size 7 containing "packet\0" */
    ENetPacket* packet = enet_packet_create(data,
        dataSize,
        ENET_PACKET_FLAG_RELIABLE);

    /* Send the packet to the peer over channel id 0. */
    /* One could also broadcast the packet by         */
    enet_host_broadcast(server, 0, packet);

    /* One could just use enet_host_service() instead. */
    enet_host_flush(server);
}

int GetRandomNumber(int max)
{
    /* initialize random seed: */
    srand(time(NULL));

    /* generate secret number between 1 and max: */
    return rand() % max + 1;
}

void SendTurnToActivePeer()
{
    // broadcast getting input from certain player
    BroadcastMessage("System Message: It is now " + GetUserNameFromPeer(activePeer) + "'s turn.");

    waitingOnPeer = true;

    // send to player it's their turn
    UserGuessGamePacket userGuessGP;
    userGuessGP.number = maxNumber;

    size_t dataSize = userGuessGP.size();
    char* data = new char[dataSize];

    UserGuessGamePacket::serialize(userGuessGP, data);

    /* Create a reliable packet of size 7 containing "packet\0" */
    ENetPacket* packet = enet_packet_create(data,
        dataSize,
        ENET_PACKET_FLAG_RELIABLE);

    /* Send the packet to the peer over channel id 0. */
    /* One could also broadcast the packet by         */
    enet_peer_send(activePeer, 0, packet);

    /* One could just use enet_host_service() instead. */
    enet_host_flush(server);
}

ENetPeer* GetNextPeer(ENetPeer* activePeer = nullptr)
{
    if (peerToNameMap.size() == 0)
    {
        return nullptr;
    }
    
    if (!activePeer)
    {
        return peerToNameMap.begin()->first;
    }

    map<ENetPeer*, string>::iterator it;

    bool passedActivePeer = false;

    for (it = peerToNameMap.begin(); it != peerToNameMap.end(); it++)
    {
        if (passedActivePeer)
        {
            return it->first;
        }

        if (it->first == activePeer)
        {
            passedActivePeer = true;
        }
    }

    return peerToNameMap.begin()->first;
}

void MoveToNextPlayer()
{
    activePeer = GetNextPeer(activePeer);
}

void BeginGame()
{
    numberToGuess = GetRandomNumber(maxNumber);

    cout << "Number to guess: " << numberToGuess << endl;

    BroadcastMessage("System Message: Starting new game. (" + to_string(GetNumberOfConnections()) + " / " + to_string(requiredNumberOfPlayersToBegin) + ")");

    MoveToNextPlayer();

    SendTurnToActivePeer();

    gameStarted = true;
}

uint32_t GetTime()
{
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

void CheckIfCanStartGame(int numberOfConnections)
{
    bool canStart = numberOfConnections >= requiredNumberOfPlayersToBegin;

    if (canStart)
    {
        BeginGame();
    }
    else
    {
        BroadcastMessage("System Message: Waiting for more players. (" + to_string(GetNumberOfConnections()) + "/" + to_string(requiredNumberOfPlayersToBegin) + ")");
    }
}

void EndGame()
{
    cout << "Game is over.";
    gameStarted = false;
    activePeer = nullptr;
    numberToGuess = 0;
    waitingOnPeer = false;

    std::this_thread::sleep_for(std::chrono::milliseconds(timeToWaitForNextGame));

    CheckIfCanStartGame(GetNumberOfConnections());
}

bool CheckIfGameOver(int guess)
{
    return guess == numberToGuess;
}

void HandleEventTypeReceiveGamePacket(ENetEvent event)
{
    GamePacket* gamePacket = (GamePacket*)event.packet->data;

    if (gamePacket)
    {
        if (gamePacket->type == PHT_UserInfo)
        {
            UserInfoGamePacket userInfoGP;
            UserInfoGamePacket::deserialize((char*)event.packet->data, event.packet->dataLength, userInfoGP);

            // save user and connectID
            auto iterator = peerToNameMap.find(event.peer);

            if (iterator == peerToNameMap.end())
            {
                peerToNameMap.insert(pair<ENetPeer*, string>(event.peer, userInfoGP.username));

                BroadcastMessage("System Message: " + userInfoGP.username + " has joined the game.");
            }

            if (!gameStarted)
            {
                CheckIfCanStartGame(GetNumberOfConnections());
            }
        }
        else if (gamePacket->type == PHT_UserGuess)
        {
            UserGuessGamePacket userGuessGP;
            UserGuessGamePacket::deserialize((char*)event.packet->data, event.packet->dataLength, userGuessGP);

            if (activePeer != nullptr && event.peer == activePeer)
            {
                if (CheckIfGameOver(userGuessGP.number))
                {
                    BroadcastMessage("System Message: Correct number guessed (" + to_string(userGuessGP.number) + 
                        ") by " + GetUserNameFromPeer(activePeer) + ". They are the winner!");

                    EndGame();
                }
                else
                {
                    BroadcastMessage("System Message: Incorrect number guessed (" + to_string(userGuessGP.number) +
                        ") by " + GetUserNameFromPeer(activePeer) + ".");

                    MoveToNextPlayer();
                    SendTurnToActivePeer();
                }

                waitingOnPeer = false;
            }
        }
    }
}

void HandleEventTypeDisconnect(ENetEvent event)
{
    int numberOfActiveConnections = GetNumberOfConnections();

    cout << "System: A peer has disconnected. Connections: "
        << numberOfActiveConnections << endl;

    string leftPlayerName = GetUserNameFromPeer(event.peer);

    auto iterator = peerToNameMap.find(event.peer);

    if (iterator != peerToNameMap.end())
    {
        BroadcastMessage("System Message: " + GetUserNameFromPeer(event.peer) + " has left the game.");
        peerToNameMap.erase(iterator);
    }

    if (event.peer == activePeer)
    {
        cout << "Active peer left.. " << leftPlayerName << endl;
        MoveToNextPlayer();

        if (activePeer)
        {
            cout << "New active peer... " << GetUserNameFromPeer(activePeer) << endl;
            SendTurnToActivePeer();
        }
    }

    if (numberOfActiveConnections == 0 && gameStarted)
    {
        EndGame();
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
            {
                int numberOfConnections = GetNumberOfConnections();

                cout << "System: A new peer has connected. Connections: "
                    << numberOfConnections << endl;

                break;
            }
            case ENET_EVENT_TYPE_RECEIVE:
            {
                HandleEventTypeReceiveGamePacket(event);

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
    }

    if (server != NULL) enet_host_destroy(server);

    return EXIT_SUCCESS;
}
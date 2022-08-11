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

void WriteLocalMessage(string message)
{
    cout << "System: " << message << endl;
}

bool CreateServer()
{
    WriteLocalMessage("Creating server...");

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

// Returns a count based on the number of peers in a CONNECTED state.
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

// Returns a username saved for a given connected peer.
string GetUserNameFromPeer(ENetPeer* peer)
{
    auto iterator = peerToNameMap.find(peer);

    if (iterator != peerToNameMap.end())
    {
        return peerToNameMap.at(peer);
    }

    return nullptr;
}

// Send a message to all connected peers.
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

// Sends a packet to the active peer and requests input.
void SendInputPromptToActivePeer()
{
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

void SendTurnToActivePeer()
{
    BroadcastMessage("System Message: It is now " + GetUserNameFromPeer(activePeer) + "'s turn.");
    SendInputPromptToActivePeer();
}

// Given the active peer, get the next peer in "line" for a turn.
ENetPeer* GetNextPeer()
{
    if (peerToNameMap.size() == 0)
    {
        return nullptr;
    }
    
    // no currently set active peer? return first in the map
    if (!activePeer)
    {
        return peerToNameMap.begin()->first;
    }

    map<ENetPeer*, string>::iterator it;

    bool passedActivePeer = false;

    for (it = peerToNameMap.begin(); it != peerToNameMap.end(); it++)
    {
        // if we've passed the active peer, return the next immediate peer
        if (passedActivePeer)
        {
            return it->first;
        }

        if (it->first == activePeer)
        {
            passedActivePeer = true;
        }
    }

    // no other peers after the active one, return first peer in map
    return peerToNameMap.begin()->first;
}

void AssignNextPeer()
{
    activePeer = GetNextPeer();
}

void BeginGame()
{
    WriteLocalMessage("Beginning game.");

    numberToGuess = GetRandomNumber(maxNumber);

    WriteLocalMessage("Number to guess: " + to_string(numberToGuess));

    BroadcastMessage("System Message: Starting new game. (" 
        + to_string(GetNumberOfConnections()) + " / " + to_string(requiredNumberOfPlayersToBegin) 
        + ")\nMinimum guess: 0, Maximum: " + to_string(maxNumber));

    AssignNextPeer();

    SendTurnToActivePeer();

    gameStarted = true;
}

// Returns current time from epoch in seconds.
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

// Reset variables. Start new game after x seconds if possible.
void EndGame()
{
    WriteLocalMessage("Game is over.");

    gameStarted = false;
    activePeer = nullptr;
    numberToGuess = 0;
    waitingOnPeer = false;

    std::this_thread::sleep_for(std::chrono::milliseconds(timeToWaitForNextGame));

    CheckIfCanStartGame(GetNumberOfConnections());
}

bool IsCorrectGuess(int guess)
{
    return guess == numberToGuess;
}

void HandleReceiveUserInfoGamePacket(ENetEvent event)
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

void HandleReceiveUserGuessGamePacket(ENetEvent event)
{
    UserGuessGamePacket userGuessGP;
    UserGuessGamePacket::deserialize((char*)event.packet->data, event.packet->dataLength, userGuessGP);

    if (activePeer != nullptr && event.peer == activePeer)
    {
        if (IsCorrectGuess(userGuessGP.number))
        {
            BroadcastMessage("System Message: Correct number guessed (" + to_string(userGuessGP.number) +
                ") by " + GetUserNameFromPeer(activePeer) + ". They are the winner!");

            EndGame();
        }
        else
        {
            BroadcastMessage("System Message: Incorrect number guessed (" + to_string(userGuessGP.number) +
                ") by " + GetUserNameFromPeer(activePeer) + ".");

            AssignNextPeer();
            SendTurnToActivePeer();
        }

        waitingOnPeer = false;
    }
}

void HandleEventTypeReceiveGamePacket(ENetEvent event)
{
    GamePacket* gamePacket = (GamePacket*)event.packet->data;

    if (gamePacket)
    {
        if (gamePacket->type == PHT_UserInfo)
        {
            HandleReceiveUserInfoGamePacket(event);
        }
        else if (gamePacket->type == PHT_UserGuess)
        {
            HandleReceiveUserGuessGamePacket(event);
        }
    }
}

void CheckIfActivePeerDisconnect(ENetEvent event, string leftPlayerName)
{
    if (event.peer == activePeer)
    {
        WriteLocalMessage("Active peer (" + leftPlayerName + ") has left.");
        
        AssignNextPeer();

        if (activePeer)
        {
            WriteLocalMessage("New active peer (" + GetUserNameFromPeer(activePeer) + ")");
            SendTurnToActivePeer();
        }
    }
}

void HandleEventTypeDisconnect(ENetEvent event)
{
    int numberOfActiveConnections = GetNumberOfConnections();

    WriteLocalMessage("A peer has disconnected. Connections: " + to_string(numberOfActiveConnections));

    string leftPlayerName = GetUserNameFromPeer(event.peer);

    auto iterator = peerToNameMap.find(event.peer);

    if (iterator != peerToNameMap.end())
    {
        BroadcastMessage("System Message: " + GetUserNameFromPeer(event.peer) + " has left the game.");
        peerToNameMap.erase(iterator);
    }
    
    CheckIfActivePeerDisconnect(event, leftPlayerName);

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
        WriteLocalMessage("An error occurred while initializing ENet.");
      
        return EXIT_FAILURE;
    }

    atexit(enet_deinitialize);

    if (!CreateServer())
    {
        fprintf(stderr,
            "An error occurred while trying to create an ENet server host.\n");
        WriteLocalMessage("An error occurred while trying to create an ENet server host.");
        ::exit(EXIT_FAILURE);
    }

    WriteLocalMessage("Server created. Waiting for connections.");

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

                WriteLocalMessage("A new peer has connected. Connections: " + to_string(numberOfConnections));

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
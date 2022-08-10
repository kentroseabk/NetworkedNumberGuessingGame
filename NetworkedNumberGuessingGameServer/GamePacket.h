#pragma once

#include <string>

using namespace std;

enum PacketHeaderType
{
    PHT_Invalid,
    PHT_UserInfo,
    PHT_UserGuess,
    PHT_Message
};

struct GamePacket
{
    GamePacket() {}
    PacketHeaderType type = PHT_Invalid;

    size_t size() const
    {
        return sizeof(type);
    }

    static size_t serialize(const GamePacket& aGamePacket, char* data)
    {
        // serialize type
        size_t bufferIdx = 0;
        size_t typeSize = sizeof(&aGamePacket.type);
        memcpy(&data[bufferIdx], &aGamePacket.type, typeSize);

        return typeSize;
    }

    static size_t deserialize(char* data, size_t dataLength, GamePacket& aGamePacket)
    {
        size_t buffIdx = 0;
        size_t typeSize = sizeof(type);
        memcpy(&aGamePacket.type, &data[buffIdx], typeSize);

        return typeSize;
    }
};

struct UserInfoGamePacket : GamePacket
{
    UserInfoGamePacket()
    {
        type = PHT_UserInfo;
    }

    string username = "";

    size_t size() const
    {
        return GamePacket::size() + username.length() + 1;
    }

    static void serialize(const UserInfoGamePacket& aUserInfoGamePacket, char* data)
    {
        size_t buffSize = aUserInfoGamePacket.size();

        size_t bufferIdx = GamePacket::serialize(aUserInfoGamePacket, data);

        // serialize username
        size_t usernameNameSize = aUserInfoGamePacket.username.length() + 1;
        memcpy(&data[bufferIdx], aUserInfoGamePacket.username.c_str(), usernameNameSize);
    }

	static void deserialize(char* data, size_t dataLength, UserInfoGamePacket& aUserInfoGamePacket)
	{
        size_t buffIdx = GamePacket::deserialize(data, dataLength, aUserInfoGamePacket);

		std::string tmp;
		for (size_t i = buffIdx; i < dataLength; ++i)
		{
			if (data[i] == '\0')
			{
				break;
			}
			else
			{
				tmp += data[i];
			}
		}

		aUserInfoGamePacket.username = tmp;
	}
};

struct MessageGamePacket : GamePacket
{
    MessageGamePacket()
    {
        type = PHT_Message;
    }

    // player joined, player left, along w/ current number of players
    string message = "";

    size_t size() const
    {
        return GamePacket::size() + message.length() + 1;
    }

    static void serialize(const MessageGamePacket& aMessageGamePacket, char* data)
    {
        size_t buffSize = aMessageGamePacket.size();

        size_t bufferIdx = GamePacket::serialize(aMessageGamePacket, data);

        // serialize message
        size_t messageSize = aMessageGamePacket.message.length() + 1;
        memcpy(&data[bufferIdx], aMessageGamePacket.message.c_str(), messageSize);
    }

    static void deserialize(char* data, size_t dataLength, MessageGamePacket& aMessageGamePacket)
    {
        size_t buffIdx = GamePacket::deserialize(data, dataLength, aMessageGamePacket);

        std::string tmp;
        for (size_t i = buffIdx; i < dataLength; ++i)
        {
            if (data[i] == '\0')
            {
                break;
            }
            else
            {
                tmp += data[i];
            }
        }

        aMessageGamePacket.message = tmp;
    }
};


struct UserGuessGamePacket : GamePacket
{
    UserGuessGamePacket()
    {
        type = PHT_UserGuess;
    }

    int number = 0;

    size_t size() const
    {
        return GamePacket::size() + sizeof(number);
    }

    static void serialize(const UserGuessGamePacket& aUserGuessGamePacket, char* data)
    {
        size_t buffSize = aUserGuessGamePacket.size();

        size_t bufferIdx = GamePacket::serialize(aUserGuessGamePacket, data);

        // serialize guess
        size_t guessSize = sizeof(number);
        memcpy(&data[bufferIdx], &aUserGuessGamePacket.number, guessSize);
    }

    static void deserialize(char* data, size_t dataLength, UserGuessGamePacket& aUserGuessGamePacket)
    {
        size_t buffIdx = GamePacket::deserialize(data, dataLength, aUserGuessGamePacket);

        size_t guessSize = sizeof(number);
        memcpy(&aUserGuessGamePacket.number, &data[buffIdx], guessSize);
    }
};
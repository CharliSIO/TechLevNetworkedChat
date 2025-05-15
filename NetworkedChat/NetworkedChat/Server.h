#pragma once
#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <vector>
#include <thread>
#include <algorithm>
#include "ThreadSafeQueue.h"

constexpr int BUFFER_SIZE = 256;

struct Message
{
	char m_Message[BUFFER_SIZE];
	char m_ID;
};

class Server
{
public:
	Server();
	~Server();

	int CreateAndBind();
	int ListenAndAccept();
	int Recieve();
	int Send();
	
	void GetCommand(Message _msg, SOCKET _cliSock);

	static void SerialiseMessage(Message _msg, char _output[])
	{
		memcpy(_output, &_msg, sizeof(Message));
	}
	static Message Deserialise(char _buff[])
	{
		Message msg;
		memcpy(&msg, _buff, sizeof(Message));
		return msg;
	}

protected:
	SOCKET m_ServerSocket;
	sockaddr_in m_ServerAddress;

	std::vector<SOCKET> m_ClientSockets;
	std::mutex m_ClientsMutex;

	std::thread m_ListenThread;

	std::thread m_RecieveThread;

	std::thread m_SendThread;

	ThreadSafeQueue<Message> m_MessageBuffer;

	std::vector<Message> m_SecretMessages;
	std::mutex m_SecretMutex;

	std::atomic_int m_NextClientID;

	bool m_AcceptingNewClients = true;
};


#pragma once
#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <vector>
#include <thread>
#include "ThreadSafeQueue.h"

constexpr int BUFFER_SIZE = 256;

class Server
{
public:
	Server();
	~Server();

	int CreateAndBind();
	int ListenAndAccept();
	int Select();
	int Recieve();
	void PrintMessage();
	int Send();

protected:
	SOCKET m_ServerSocket;
	sockaddr_in m_ServerAddress;

	std::vector<SOCKET> m_ClientSockets;
	//ThreadSafeQueue<SOCKET> m_ClientSockets;

	std::thread m_ListenThread;
	std::mutex m_ListenMutex;

	std::thread m_RecieveThread;

	std::thread m_SendThread;

	ThreadSafeQueue<std::string> m_MessageBuffer;


	bool m_AcceptingNewClients = true;
};


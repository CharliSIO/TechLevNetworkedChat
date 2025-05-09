#pragma once
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <vector>
#include <thread>
#include <mutex>

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

	std::thread m_ListenThread;


	bool m_AcceptingNewClients = true;
};


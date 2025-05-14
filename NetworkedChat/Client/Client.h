#pragma once
#pragma once
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <vector>
#include <thread>
#include <mutex>

constexpr int BUFFER_SIZE = 256;

class Client
{
public:
	Client();
	~Client();

	int CreateAndConnect();
	int Send();
	int Recieve();

protected:
	SOCKET m_Socket;
	sockaddr_in m_Address; 
	sockaddr_in m_RecieveAddress;

	std::thread m_RecieveThread;
	std::thread m_SendThread;

	const int m_BufferSize = 256;
};


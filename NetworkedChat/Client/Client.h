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

struct Message
{
	char m_Message[BUFFER_SIZE];
	char m_ID;
};

class Client
{
public:
	Client();
	~Client();
	void Join();

	int CreateAndConnect();
	int Send();
	int Recieve();


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

	std::atomic_bool m_ShouldQuit = false;

protected:
	SOCKET m_Socket;
	sockaddr_in m_Address; 
	sockaddr_in m_RecieveAddress;

	std::thread m_RecieveThread;
	std::thread m_SendThread;


	char m_ID = 0;

	const int m_BufferSize = 256;
};


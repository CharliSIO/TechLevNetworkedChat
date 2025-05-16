#pragma once
#include "Server.h"

bool InitWSA();

// Working through exercises - split into classes etc
int main()
{
	InitWSA();
	
	Server gServer;
	if (gServer.ListenAndAccept() == -1) return -1;
	if (gServer.Recieve() == -1) return -1;
	
	gServer.Join();

	int pause = 0;
	std::cin >> pause;
	WSACleanup();
	return 0;
}

bool InitWSA()
{
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2); // winsock version 2.2

	int result = WSAStartup(wVersionRequested, &wsaData);
	if (result != 0)
	{
		printf("WSAStartUp failed %d\n", result);
		return false;
	}

	// check version exists
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Error: Version is not available\n");
		return false;
	}

	return true;
}

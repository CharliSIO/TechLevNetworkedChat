#pragma once
#include "Client.h"

bool InitWSA();

int main()
{
	InitWSA();
	Client Client1;
	
	Client1.Join();
	
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


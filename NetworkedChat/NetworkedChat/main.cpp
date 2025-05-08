#pragma once
#include "Server.h"

bool InitWSA();
int Client();

// Working through exercises - split into classes etc
int main()
{
	InitWSA();
	std::cout << "Run server or client?\n";
	std::cout << "1) Server\n";
	std::cout << "2) Client\n" << std::endl;
	int iChoice = 0;
	std::cin >> iChoice;
	if (iChoice == 1)
	{
		Server gServer;
		if (gServer.CreateAndBind() == -1) return -1;
		if (gServer.ListenAndAccept() == -1) return -1;
		if (gServer.Recieve() == -1) return -1;
	}
	else
	{
		Client();
	}

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

int Client()
{
	SOCKET clientSocket;

	// create socket
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET)
	{
		printf("Error in socket(). Error code: %d\n", WSAGetLastError());
		return -1;
	}

	// set address
	sockaddr_in clientAddress;
	clientAddress.sin_family = AF_INET;
	clientAddress.sin_port = htons(20001); // different from server
	clientAddress.sin_addr.S_un.S_addr = INADDR_ANY;

	//// bind
	//int status = bind(clientSocket, (sockaddr*)&clientAddress, sizeof(clientAddress));
	//if (status == SOCKET_ERROR)
	//{
	//	printf("Error in bind(). Error code: %d\n", WSAGetLastError());
	//	return -1;
	//}

	sockaddr_in recvAddress;
	recvAddress.sin_family = AF_INET;
	recvAddress.sin_port = htons(20000);
	// 127.0.0.1 ip is local host. change for non-local host
	InetPton(AF_INET, L"127.0.0.1", &recvAddress.sin_addr.S_un.S_addr);

	int status = connect(clientSocket, (sockaddr*)&recvAddress, sizeof(recvAddress));
	if (status == SOCKET_ERROR)
	{
		printf("Error in connect(). Error code: %d\n", WSAGetLastError());
		return -1;
	}

	// write to a socket ---- SEND MESSAGE
	char buffer[256]; // BUFFER_SIZE in slides - undefined - need to define? 

	while (true)
	{
		printf("Enter a message to send: \n");
		std::cin.getline(buffer, 256); // BUFFER_SIZE

		status = send(clientSocket, buffer, strlen(buffer), 0);
		if (status == SOCKET_ERROR)
		{
			printf("Error in send(). Error code: %d\n", WSAGetLastError());
			break;
		}
	}

	closesocket(clientSocket);
	return false;
}

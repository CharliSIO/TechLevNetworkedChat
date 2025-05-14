#include "Server.h"

Server::Server()
{
	CreateAndBind();
}

Server::~Server()
{
	if (m_ListenThread.joinable())
	{
		m_ListenThread.join();
	}
	if (m_RecieveThread.joinable())
	{
		m_RecieveThread.join();
	}
	if (m_SendThread.joinable())
	{
		m_SendThread.join();
	}

	for (SOCKET cliSocket : m_ClientSockets)
	{
		closesocket(cliSocket);
	}
	closesocket(m_ServerSocket);
}

int Server::CreateAndBind()
{
	m_ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (m_ServerSocket == INVALID_SOCKET)
	{
		printf("Error in socket(). Error code: %d\n", WSAGetLastError());
		closesocket(m_ServerSocket);
		WSACleanup();
		return -1;
	}

	int sockOpt = 1;

	m_ServerAddress.sin_family = AF_INET;
	m_ServerAddress.sin_port = htons(20000);
	m_ServerAddress.sin_addr.S_un.S_addr = INADDR_ANY;

	int bound = bind(m_ServerSocket, (sockaddr*)&m_ServerAddress, sizeof(m_ServerAddress));
	if (bound == SOCKET_ERROR)
	{
		printf("Error in bind(). Error code: %d\n", WSAGetLastError());
		closesocket(m_ServerSocket);
		WSACleanup();
		return -1;
	}

	// Create thread for listening and accepting new clients
	m_ListenThread = std::thread(&Server::ListenAndAccept, this);
	m_RecieveThread = std::thread(&Server::Recieve, this);
	m_SendThread = std::thread(&Server::Send, this);

	return 0;
}

int Server::ListenAndAccept()
{
	while (m_AcceptingNewClients)
	{
		int iClients = 0;
		int status = listen(m_ServerSocket, 5);
		if (status == SOCKET_ERROR)
		{
			printf("Error in listen(). Error code: %d\n", WSAGetLastError());
			WSACleanup();
			return -1;
		}

		// -- SELECT FOR MULTIPLE CLIENTS
		TIMEVAL tWait; tWait.tv_sec = 1; tWait.tv_usec = 0;
		int iNumSocksReady = 0; int iCount = 0;
		while (iNumSocksReady == 0)
		{
			fd_set readSet; FD_ZERO(&readSet); // initialise readSet
			FD_SET(m_ServerSocket, &readSet); // add serverSocket to readSet
			iNumSocksReady = select(0, &readSet, NULL, NULL, &tWait); // run selected ONLY on readSet 
		}

		for (int i = iNumSocksReady; i > 0; i--)
		{
			sockaddr_in cliAddress;
			int addressLength = sizeof(cliAddress);
			SOCKET cliSocket = accept(m_ServerSocket, (sockaddr*)&cliAddress, &addressLength);
			if (cliSocket == INVALID_SOCKET)
			{
				printf("Error in accept(). Error code: %d\n", WSAGetLastError());
				WSACleanup();
				return -1;
			}
			m_ClientSockets.push_back(std::move(cliSocket));
			m_MessageBuffer.Push("Client connected!\n\n");
		}
	}

	return 0;
}

int Server::Recieve()
{
	char buffer[BUFFER_SIZE];

	while (true)
	{
		TIMEVAL tWait; tWait.tv_sec = 1; tWait.tv_usec = 0;
		int iNumSocksReady = 0; int iCount = 0;
		fd_set readSet;

		while (iNumSocksReady == 0)
		{
			FD_ZERO(&readSet);
			for (SOCKET cliSocket : m_ClientSockets)
			{
				FD_SET(cliSocket, &readSet);
			}
			iNumSocksReady = select(0, &readSet, NULL, NULL, &tWait);
		}
		//printf("\n");

		for (SOCKET cliSocket : m_ClientSockets)
		{
			if (FD_ISSET(cliSocket, &readSet))
			{
				int rcv = recv(cliSocket, buffer, 255, 0);
				if (rcv == SOCKET_ERROR)
				{
					printf("Error in recv(). Error code: %d\n", WSAGetLastError());
					continue;
				}

				buffer[rcv] = '\0';
				m_MessageBuffer.Push(("Message: %s\n\n", buffer));
			}
		}
	}

	for (SOCKET cliSocket : m_ClientSockets)
	{
		closesocket(cliSocket);
	}
	closesocket(m_ServerSocket);
	return 0;
}

int Server::Send()
{
	// write to a socket ---- SEND MESSAGE
	char buffer[BUFFER_SIZE];

	while (true)
	{
		std::string message = "";

		if (m_MessageBuffer.BlockPop(message))
		{
			strcpy_s(buffer, message.c_str());

			for (SOCKET cliSocket : m_ClientSockets)
			{
				int status = send(cliSocket, buffer, strlen(buffer), 0);
				if (status == SOCKET_ERROR)
				{
					printf("Error in send(). Error code: %d\n", WSAGetLastError());
					break;
				}
			}
		}
	}

	return 0;
}

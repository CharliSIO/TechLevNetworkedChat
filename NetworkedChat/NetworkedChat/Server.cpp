#include "Server.h"

Server::Server()
{
	CreateAndBind();
}

Server::~Server()
{
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

	return 0;
}

int Server::ListenAndAccept()
{
	while (m_AcceptingNewClients)
	{
		int iClients = 0;
		printf("Listening...\n");
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
			printf("\rTime waiting for connection: %i", iCount);
			iCount++;
			fd_set readSet; FD_ZERO(&readSet); // initialise readSet
			FD_SET(m_ServerSocket, &readSet); // add serverSocket to readSet
			iNumSocksReady = select(0, &readSet, NULL, NULL, &tWait); // run selected ONLY on readSet 
		}

		printf("Accepting...");
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
		}

		m_AcceptingNewClients = m_ClientSockets.size() < 3.0f;
	}

	return 0;
}

int Server::Recieve()
{
	printf("Recieving...\n");
	char buffer[BUFFER_SIZE];
	while (true)
	{
		TIMEVAL tWait; tWait.tv_sec = 1; tWait.tv_usec = 0;
		int iNumSocksReady = 0; int iCount = 0;
		fd_set readSet;

		while (iNumSocksReady == 0)
		{
			printf("\rWaiting for a message... Total time waiting: %i", iCount);
			iCount++;
			FD_ZERO(&readSet);
			for (SOCKET cliSocket : m_ClientSockets)
			{
				FD_SET(cliSocket, &readSet);
			}
			iNumSocksReady = select(0, &readSet, NULL, NULL, &tWait);
		}
		printf("\n");

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
				printf("Message: %s\n\n", buffer);
			}
		}
		Send();
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

	//while (true)
	//{
		printf("Enter a message to send: \n");
		std::cin.getline(buffer, (static_cast<std::streamsize>(BUFFER_SIZE) - 1));

		for (SOCKET cliSocket : m_ClientSockets)
		{
			int status = send(cliSocket, buffer, strlen(buffer), 0);
			if (status == SOCKET_ERROR)
			{
				printf("Error in send(). Error code: %d\n", WSAGetLastError());
				break;
			}
		}
	//}

	return 0;
}

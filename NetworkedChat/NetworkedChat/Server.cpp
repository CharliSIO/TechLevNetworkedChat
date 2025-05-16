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
	closesocket(m_ServerSocket);
	WSACleanup();
}

void Server::Join()
{
	if (m_ListenThread.joinable()) m_ListenThread.join();
	if (m_RecieveThread.joinable()) m_RecieveThread.join();
	if (m_SendThread.joinable()) m_SendThread.join();
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

	char strAddr[16];
	//sockaddr_in serverAddrIn = (sockaddr_in*)
	inet_ntop(AF_INET, &(m_ServerAddress.sin_addr), strAddr, sizeof(strAddr));
	printf("Server IPv4 address: %s\n", strAddr);

	// Create thread for listening and accepting new clients
	m_ListenThread = std::thread(&Server::ListenAndAccept, this);
	m_RecieveThread = std::thread(&Server::Recieve, this);
	m_SendThread = std::thread(&Server::Send, this);

	return 0;
}

int Server::ListenAndAccept()
{
	while (m_AcceptingNewClients && !m_ShouldQuit)
	{
		int iClients = 0;
		int status = listen(m_ServerSocket, 5);
		if (status == SOCKET_ERROR)
		{
			printf("Error in listen(). Error code: %d\n", WSAGetLastError());
			return -1;
		}

		// -- SELECT FOR MULTIPLE CLIENTS
		TIMEVAL tWait; tWait.tv_sec = 1; tWait.tv_usec = 0;
		int iNumSocksReady = 0; int iCount = 0;
		while ( !m_ShouldQuit && iNumSocksReady == 0)
		{
			fd_set readSet; FD_ZERO(&readSet); // initialise readSet
			FD_SET(m_ServerSocket, &readSet); // add serverSocket to readSet
			iNumSocksReady = select(0, &readSet, NULL, NULL, &tWait); // run selected ONLY on readSet 
		}

		for (int i = iNumSocksReady; i > 0; i--)
		{
			sockaddr_in cliAddress;
			int addressLength = sizeof(cliAddress);
			if (m_ShouldQuit) return -1;
			SOCKET cliSocket = accept(m_ServerSocket, (sockaddr*)&cliAddress, &addressLength);
			if (cliSocket == INVALID_SOCKET)
			{
				if (m_ShouldQuit) return -1;
				printf("Error in accept(). Error code: %d\n", WSAGetLastError());
				return -1;
			}

			std::unique_lock<std::mutex> uniqueLock(m_ClientsMutex);
			m_ClientSockets.push_back(std::move(cliSocket));

			char idbuffer[sizeof(Message)];
			Message idmessage;

			idmessage.m_ID = (char)(m_ClientSockets.size() + 48);
			uniqueLock.unlock();

			strcpy_s(idmessage.m_Message, "CMD_ID");
			SerialiseMessage(idmessage, idbuffer);

			int status = send(cliSocket, idbuffer, sizeof(Message), 0);
			if (status == SOCKET_ERROR)
			{
				printf("Error in send(). Error code: %d\n", WSAGetLastError());
			}

			char strAddr[16];
			inet_ntop(AF_INET, &cliAddress.sin_addr, strAddr, sizeof(strAddr));
			printf("Connection established with client at %s\n", strAddr);

			char buffer[sizeof(Message)];
			Message message; message.m_ID = '0';
			strcpy_s(message.m_Message, "Client connected!\n\n");
			m_MessageBuffer.Push(message);
		}
	}

	return 0;
}

int Server::Recieve()
{
	char buffer[sizeof(Message)];

	while (!m_ShouldQuit)
	{
		TIMEVAL tWait; tWait.tv_sec = 1; tWait.tv_usec = 0;
		int iNumSocksReady = 0; int iCount = 0;
		fd_set readSet;

		while (iNumSocksReady == 0)
		{
			std::unique_lock<std::mutex> uniqueLock(m_ClientsMutex);
			FD_ZERO(&readSet);
			for (SOCKET cliSocket : m_ClientSockets)
			{
				FD_SET(cliSocket, &readSet);
			}
			uniqueLock.unlock();
			iNumSocksReady = select(0, &readSet, NULL, NULL, &tWait);
		}

		std::unique_lock<std::mutex> uniqueLock(m_ClientsMutex);
		for (auto cliSocket = m_ClientSockets.begin(); cliSocket != m_ClientSockets.end();)
		{
			if (FD_ISSET(*cliSocket, &readSet))
			{
				int rcv = recv(*cliSocket, buffer, sizeof(Message), 0);
				if (rcv == 0)
				{
					// client has disconnected
					if (shutdown(*cliSocket, SD_BOTH) == SOCKET_ERROR)
					{
						printf("Error in shutdown(). Error code: %d\n", WSAGetLastError());
					}
					closesocket(*cliSocket); 
					cliSocket = m_ClientSockets.erase(cliSocket);
					continue;
				}
				else if (rcv == SOCKET_ERROR)
				{
					if (shutdown(*cliSocket, SD_BOTH) == SOCKET_ERROR)
					{
						printf("Error in shutdown(). Error code: %d\n", WSAGetLastError());
					}
					printf("Error in recv(). Error code: %d\n", WSAGetLastError());
					closesocket(*cliSocket);
					cliSocket = m_ClientSockets.erase(cliSocket);
					continue;
				}
				
				Message message = Deserialise(buffer);
				message.m_Message[rcv - 2] = '\0';
				if (message.m_Message[0] == '/')
				{
					if (GetCommand(message, *cliSocket) == -1)
					{
						if (m_ShouldQuit)
						{
							uniqueLock.unlock(); return 0;
						}
						else break;
					}
				}
				else m_MessageBuffer.Push(message);
			}
			cliSocket++;
		}
		uniqueLock.unlock();
	}
	return 0;
}

int Server::Send()
{
	// write to a socket ---- SEND MESSAGE
	char buffer[sizeof(Message)];

	while (!m_ShouldQuit)
	{
		Message message;

		if (m_MessageBuffer.BlockPop(message))
		{
			SerialiseMessage(message, buffer);

			std::unique_lock<std::mutex> uniqueLock(m_ClientsMutex);
			for (SOCKET cliSocket : m_ClientSockets)
			{
				int status = send(cliSocket, buffer, sizeof(buffer), 0);
				if (status == SOCKET_ERROR)
				{
					printf("Error in send(). Error code: %d\n", WSAGetLastError());
					break;
				}
			}
			uniqueLock.unlock();
		}
	}

	return 0;
}

int Server::GetCommand(Message _msg, SOCKET _cliSock)
{
	std::string message = _msg.m_Message;

	std::string command = message.substr(message.find('/'), message.find(' '));
	message = message.substr(message.find(' ') + 1);

	if (command == "/CAPITALIZE" || command == "/CAPITALISE")
	{
		std::transform(message.begin(), message.end(), message.begin(), 
			[](unsigned char c) {return std::toupper(c); });
		
		char buffer[sizeof(Message)];
		strcpy_s(_msg.m_Message, message.c_str());
		SerialiseMessage(_msg, buffer);

		int status = send(_cliSock, buffer, sizeof(buffer), 0);
		if (status == SOCKET_ERROR)
		{
			printf("Error in send(). Error code: %d\n", WSAGetLastError());
		}
	}
	else if (command == "/PUT")
	{
		std::lock_guard<std::mutex> secretLock(m_SecretMutex);
		strcpy_s(_msg.m_Message, message.c_str());
		m_SecretMessages.push_back(_msg);

		char buffer[sizeof(Message)];
		Message serverResponse;

		strcpy_s(serverResponse.m_Message, "Your secret is safe... for now :)");
		SerialiseMessage(serverResponse, buffer);

		int status = send(_cliSock, buffer, sizeof(buffer), 0);
		if (status == SOCKET_ERROR)
		{
			printf("Error in send(). Error code: %d\n", WSAGetLastError());
		}
	}
	else if (command == "/GET")
	{
		std::lock_guard<std::mutex> secretLock(m_SecretMutex);
		auto secret = std::find_if(m_SecretMessages.begin(), m_SecretMessages.end(),
			[&](const Message& _secretMsg) {return _msg.m_ID == _secretMsg.m_ID; });

		char buffer[sizeof(Message)];
		SerialiseMessage(*secret, buffer);

		int status = send(_cliSock, buffer, sizeof(buffer), 0);
		if (status == SOCKET_ERROR)
		{
			printf("Error in send(). Error code: %d\n", WSAGetLastError());
		}
	}
	else if (command == "/QUIT")
	{
		char buffer[sizeof(Message)];

		strcpy_s(_msg.m_Message, "CMD_QUIT");
		SerialiseMessage(_msg, buffer);

		int status = send(_cliSock, buffer, sizeof(buffer), 0);
		if (status == SOCKET_ERROR)
		{
			printf("Error in send(). Error code: %d\n", WSAGetLastError());
		}

		for (auto cliSocket = m_ClientSockets.begin(); cliSocket != m_ClientSockets.end();)
		{
			if (_cliSock == *cliSocket)
			{
				// client has disconnected
				if (shutdown(_cliSock, SD_BOTH) == SOCKET_ERROR)
				{
					printf("Error in shutdown(). Error code: %d\n", WSAGetLastError());
				}
				closesocket(_cliSock);
				cliSocket = m_ClientSockets.erase(cliSocket);
				continue;
			}
			cliSocket++;
		}
		Message serverResponse;
		sprintf_s(serverResponse.m_Message, sizeof(serverResponse.m_Message), "Client %c is disconnecting...", _msg.m_ID);
		SerialiseMessage(serverResponse, buffer);

		if (m_ClientSockets.size() > 0)
		{
			for (auto cliSocket = m_ClientSockets.begin(); cliSocket != m_ClientSockets.end();)
			{
				int status = send(*cliSocket, buffer, sizeof(buffer), 0);
				if (status == SOCKET_ERROR)
				{
					printf("Error in send(). Error code: %d\n", WSAGetLastError());
					break;
				}
				cliSocket++;
			}
		}
		else
		{
			m_AcceptingNewClients = false;
			m_ShouldQuit = true;
			m_MessageBuffer.UnblockAll();
			printf("No clients connecting, closing applicaton...\n");
			closesocket(m_ServerSocket);
			printf("Server shut Successfully.");
		}
		return -1;
	}
	return 1;
}

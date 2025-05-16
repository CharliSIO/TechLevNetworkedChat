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

// join all threads
void Server::Join()
{
	if (m_ListenThread.joinable()) m_ListenThread.join();
	if (m_RecieveThread.joinable()) m_RecieveThread.join();
	if (m_SendThread.joinable()) m_SendThread.join();
}

// create and bind server socket
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

// listen for client requesting a connection
int Server::ListenAndAccept()
{
	while (m_AcceptingNewClients && !m_ShouldQuit)
	{
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
			m_ClientSockets.push_back(std::make_pair(m_NextID, std::move(cliSocket)));

			char idbuffer[sizeof(Message)];
			Message idmessage;

			idmessage.m_ID = m_NextID;
			m_NextID++;
			uniqueLock.unlock();

			strcpy_s(idmessage.m_Message, "CMD_ID");
			SerialiseMessage(idmessage, idbuffer);

			// send message to new client telling them their ID
			int status = send(cliSocket, idbuffer, sizeof(Message), 0);
			if (status == SOCKET_ERROR)
			{
				printf("Error in send(). Error code: %d\n", WSAGetLastError());
			}

			// announce to all clients that new client connected
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

// recieve messages from the client as message struct, deserialise then check if command needs to be run
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
			for (auto cliSocket = m_ClientSockets.begin(); cliSocket != m_ClientSockets.end(); cliSocket++)
			{
				FD_SET(cliSocket->second, &readSet);
			}
			uniqueLock.unlock();
			iNumSocksReady = select(0, &readSet, NULL, NULL, &tWait);
		}

		std::unique_lock<std::mutex> uniqueLock(m_ClientsMutex);
		for (auto cliSocket = m_ClientSockets.begin(); cliSocket != m_ClientSockets.end();)
		{
			if (FD_ISSET(cliSocket->second, &readSet))
			{
				int rcv = recv(cliSocket->second, buffer, sizeof(Message), 0);
				if (rcv == 0)
				{
					// client has disconnected
					if (shutdown(cliSocket->second, SD_BOTH) == SOCKET_ERROR)
					{
						printf("Error in shutdown(). Error code: %d\n", WSAGetLastError());
					}
					closesocket(cliSocket->second);
					cliSocket = m_ClientSockets.erase(cliSocket);
					continue;
				}
				else if (rcv == SOCKET_ERROR)
				{
					if (shutdown(cliSocket->second, SD_BOTH) == SOCKET_ERROR)
					{
						printf("Error in shutdown(). Error code: %d\n", WSAGetLastError());
					}
					printf("Error in recv(). Error code: %d\n", WSAGetLastError());
					closesocket(cliSocket->second);
					cliSocket = m_ClientSockets.erase(cliSocket);
					continue;
				}
				
				Message message = Deserialise(buffer);
				if (rcv > BUFFER_SIZE) message.m_Message[(rcv - 2)] = '\0';
				else message.m_Message[(rcv - 1)] = '\0';

				if (message.m_Message[0] == '/')
				{
					if (GetCommand(message, cliSocket->second) == -1)
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

// send the top message from message buffer
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
			for (auto cliSocket = m_ClientSockets.begin(); cliSocket != m_ClientSockets.end(); cliSocket++)
			{
				int status = send(cliSocket->second, buffer, sizeof(buffer), 0);
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

// Check what command the client has requested
int Server::GetCommand(Message _msg, SOCKET _cliSock)
{
	std::string message = _msg.m_Message;

	std::string command = message.substr(message.find('/'), message.find(' '));
	message = message.substr(message.find(' ') + 1);

	if (command == "/CAPITALIZE" || command == "/CAPITALISE")
	{
		// iterate through message and convert to uppercase
		std::transform(message.begin(), message.end(), message.begin(), 
			[](unsigned char c) {return std::toupper(c); });
		
		char buffer[sizeof(Message)];
		strcpy_s(_msg.m_Message, message.c_str()); // copy string back into msg
		SerialiseMessage(_msg, buffer);

		m_MessageBuffer.Push(_msg); // add it to buffer so all clients recieve
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

		int status = send(_cliSock, buffer, sizeof(buffer), 0); // send to socket command recieved from, not all sockets
		if (status == SOCKET_ERROR)
		{
			printf("Error in send(). Error code: %d\n", WSAGetLastError());
		}
	}
	else if (command == "/GET")
	{
		std::lock_guard<std::mutex> secretLock(m_SecretMutex);

		// get secret message belonging to requested client
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
	else if (command == "/WHISPER")
	{
		char buffer[sizeof(Message)];

		char secondID = message[0];
		message = message.substr(1);

		std::string whisper = "(Whisper)" + message;
		strcpy_s(_msg.m_Message, whisper.c_str());
		SerialiseMessage(_msg, buffer);

		auto whisperSocket = std::find_if(m_ClientSockets.begin(), m_ClientSockets.end(),
			[&](std::pair<char, SOCKET>& _cli) {return secondID == _cli.first; });

		int status = send(whisperSocket->second, buffer, sizeof(buffer), 0);
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
			if (_cliSock == cliSocket->second)
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
				int status = send(cliSocket->second, buffer, sizeof(buffer), 0);
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
			// shut down server if no clients left connected
			m_AcceptingNewClients = false;
			m_ShouldQuit = true;
			m_MessageBuffer.UnblockAll();
			printf("No clients connecting, closing applicaton...\n");
			closesocket(m_ServerSocket);
			printf("Server shut Successfully.");
		}
		return -1;
	}
	else
	{
		char buffer[sizeof(Message)];
		Message serverResponse;

		strcpy_s(serverResponse.m_Message, "Invalid command, please enter a valid command :(");
		SerialiseMessage(serverResponse, buffer);

		int status = send(_cliSock, buffer, sizeof(buffer), 0);
		if (status == SOCKET_ERROR)
		{
			printf("Error in send(). Error code: %d\n", WSAGetLastError());
		}
	}
	return 1;
}

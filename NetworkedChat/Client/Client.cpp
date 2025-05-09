#include "Client.h"

Client::Client()
{
    CreateAndConnect();
}

Client::~Client()
{
    closesocket(m_Socket);
}

int Client::CreateAndConnect()
{
    m_Socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_Socket == INVALID_SOCKET)
    {
        printf("Error in socket(). Error code: %d\n", WSAGetLastError());
        return -1;
    }

    m_Address.sin_family = AF_INET;
    m_Address.sin_port = htons(20001); // different from server
    m_Address.sin_addr.S_un.S_addr = INADDR_ANY;

    m_RecieveAddress.sin_family = AF_INET;
    m_RecieveAddress.sin_port = htons(20000);
    // 127.0.0.1 ip is local host. change for non-local host
    InetPton(AF_INET, L"127.0.0.1", &m_RecieveAddress.sin_addr.S_un.S_addr);

    int status = connect(m_Socket, (sockaddr*)&m_RecieveAddress, sizeof(m_RecieveAddress));
    if (status == SOCKET_ERROR)
    {
        printf("Error in connect(). Error code: %d\n", WSAGetLastError());
        return -1;
    }

    return 0;
}

int Client::Send()
{
    // write to a socket ---- SEND MESSAGE
    char buffer[BUFFER_SIZE];

    while (true)
    {
        printf("Enter a message to send: \n");
        std::cin.getline(buffer, (static_cast<std::streamsize>(BUFFER_SIZE) - 1)); 

        int status = send(m_Socket, buffer, strlen(buffer), 0);
        if (status == SOCKET_ERROR)
        {
            printf("Error in send(). Error code: %d\n", WSAGetLastError());
            break;
        }
        Recieve();
    }
    return 0;
}

int Client::Recieve()
{
    printf("Recieving...\n");
    char buffer[BUFFER_SIZE];
    //while (true)
    //{
        TIMEVAL tWait; tWait.tv_sec = 1; tWait.tv_usec = 0;
        int iNumSocksReady = 0; int iCount = 0;
        fd_set readSet;

        while (iNumSocksReady == 0)
        {
            printf("\rWaiting for a message... Total time waiting: %i", iCount);
            iCount++;
            FD_ZERO(&readSet);
            FD_SET(m_Socket, &readSet);
            iNumSocksReady = select(0, &readSet, NULL, NULL, &tWait);
        }
        printf("\n");

        if (FD_ISSET(m_Socket, &readSet))
        {
            int rcv = recv(m_Socket, buffer, 255, 0);
            if (rcv == SOCKET_ERROR)
            {
                printf("Error in recv(). Error code: %d\n", WSAGetLastError());
                //continue;
            }

            buffer[rcv] = '\0';
            printf("Message: %s\n\n", buffer);
        }
   // }

    return 0;
}

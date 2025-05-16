#include "Client.h"

Client::Client()
{
    CreateAndConnect();
}

Client::~Client()
{
    if (m_RecieveThread.joinable()) m_RecieveThread.join();
    if (m_SendThread.joinable()) m_SendThread.join();

    closesocket(m_Socket);
    WSACleanup();
}

// join threads
void Client::Join()
{
    if (m_RecieveThread.joinable()) m_RecieveThread.join();
    if (m_SendThread.joinable()) m_SendThread.join();
}

// create socket and connect to server
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
    char addr[16];
    std::cout << "Enter server IP address: " << std::endl;
    std::cin.getline(addr, sizeof(addr));

    InetPtonA(AF_INET, addr, &m_RecieveAddress.sin_addr); // convert text to IPv4 to connect to server

    int status = connect(m_Socket, (sockaddr*)&m_RecieveAddress, sizeof(m_RecieveAddress));
    if (status == SOCKET_ERROR)
    {
        printf("Error in connect(). Error code: %d\n", WSAGetLastError());
        return -1;
    }
    m_RecieveThread = std::thread(&Client::Recieve, this);
    m_SendThread = std::thread(&Client::Send, this);

    return 0;
}

// get client's message and send it to server
int Client::Send()
{
    // write to a socket ---- SEND MESSAGE
    char buffer[sizeof(Message)];
    Message mMessage;

    while (!m_ShouldQuit)
    {
        printf("Enter a message to send: \n");
        std::cin.getline(mMessage.m_Message, (static_cast<std::streamsize>(BUFFER_SIZE - 1))); 
        mMessage.m_ID = m_ID;

        SerialiseMessage(mMessage, buffer); // serialise ID + text message

        int status = send(m_Socket, buffer, strlen(buffer), 0);
        if (status == SOCKET_ERROR)
        {
            printf("Error in send(). Error code: %d\n", WSAGetLastError());
            break;
        }
    }
    return 0;
}

// recieve messages from server, check if command has been called
int Client::Recieve()
{
    char buffer[sizeof(Message)];
    while (!m_ShouldQuit)
    {
        TIMEVAL tWait; tWait.tv_sec = 1; tWait.tv_usec = 0;
        int iNumSocksReady = 0; int iCount = 0;
        fd_set readSet;

        while (iNumSocksReady == 0)
        {

            FD_ZERO(&readSet);
            FD_SET(m_Socket, &readSet);
            iNumSocksReady = select(0, &readSet, NULL, NULL, &tWait);
        }

        if (FD_ISSET(m_Socket, &readSet))
        {
            int rcv = recv(m_Socket, buffer, sizeof(buffer), 0);
            if (rcv == 0)
            {
                break;
            }
            if (rcv == SOCKET_ERROR)
            {
                printf("Error in recv(). Error code: %d\n", WSAGetLastError());
                if (shutdown(m_Socket, SD_BOTH) == SOCKET_ERROR)
                {
                    printf("Error in shutdown(). Error code: %d\n", WSAGetLastError());
                    continue;
                }
                closesocket(m_Socket);
                printf("Quit Successfully. Press [ENTER] to close window. ");
                m_ShouldQuit = true;
                return -1;
            }

            Message message = Deserialise(buffer);
            if (rcv > BUFFER_SIZE) message.m_Message[(rcv - 2)] = '\0';
            else message.m_Message[(rcv - 1)] = '\0';

            std::string str = message.m_Message;

            // if client is being told to quit then close sockets and shut down
            if (str == "CMD_QUIT")
            {
                if (shutdown(m_Socket, SD_BOTH) == SOCKET_ERROR)
                {
                    printf("Error in shutdown(). Error code: %d\n", WSAGetLastError());
                    continue;
                }
                closesocket(m_Socket);
                printf("Quit Successfully. Press [ENTER] to close window. ");
                m_ShouldQuit = true;
            }
            else if (str == "CMD_ID") // if being told to change id, change it
            {
                m_ID = message.m_ID;
            }
            else printf("%c: %s\n", message.m_ID, message.m_Message); // print the message
        }
    }
    return 0;
}


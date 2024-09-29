#pragma once
#include <WinSock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <iostream>

#pragma comment(lib,"ws2_32.lib")

using namespace std;

void HandleError(const char* cause)
{
	int32_t errCode = ::WSAGetLastError();
	cout << "ErrorCode : " << errCode << endl;
}


int main()
{


	WSAData wsaData;
	if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		cout << "start up 에러" << endl;

	SOCKET clientSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == INVALID_SOCKET)
		return 0;


	u_long on = 1;
	if (::ioctlsocket(clientSocket, FIONBIO, &on) == INVALID_SOCKET)
		return 0;

	SOCKADDR_IN serverAddr;
	::memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
	serverAddr.sin_port = ::htons(5252);

	//Connect
	while (true)
	{
		if (::connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		{
			//원래 블록했어야하지만 논블로킹으로인해 통과가 된경우
			if (::WSAGetLastError() == WSAEWOULDBLOCK)
				continue;
			//이미 연결된상태라면 break;
			if (::WSAGetLastError() == WSAEISCONN)
				break;
			//Error
			break;
		}
	}

	cout << "Connected to Server!" << endl;

	char sendBuffer[100] = "Hello World";

	while (true)
	{
		if (::send(clientSocket, sendBuffer, sizeof(sendBuffer), 0) == SOCKET_ERROR)
		{
			//원래 블록이 되어야하지만 논블로킹하라며?
			if (::WSAGetLastError() == WSAEWOULDBLOCK)
				continue;

			//Error
			break;
		}

		cout << "Send Data ! Len = " << sizeof(sendBuffer) << endl;

		while (true)
		{
			char recvBuffer[1000];
			int32_t recvLen = ::recv(clientSocket, recvBuffer, sizeof(recvBuffer), 0);
			if (recvLen == SOCKET_ERROR)
			{
				//원래 블록했어야하지만 논블로킹으로인해 통과가 된경우
				if (::WSAGetLastError() == WSAEWOULDBLOCK)
					continue;

				//error
				break;
			}
			else if (recvLen == 0)
			{
				//연결 끊김
				break;
			}
			cout << "Recv Data Len = " << recvLen << endl;
			break;
		}
		this_thread::sleep_for(1s);
	}

	::closesocket(clientSocket);
	::WSACleanup();
}
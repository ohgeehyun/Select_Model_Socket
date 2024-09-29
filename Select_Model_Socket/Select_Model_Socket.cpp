#pragma once
#include <WinSock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <iostream>
#include <vector>

#pragma comment(lib,"ws2_32.lib")

using namespace std;


void HandleError(const char* cause)
{
	int32_t errCode = ::WSAGetLastError();
	cout << "ErrorCode : " << errCode << endl;
}

const int32_t BUFSIZE = 1000;

struct Session
{
	SOCKET socket = INVALID_SOCKET;
	char recvBuffer[BUFSIZE] = {};
	int32_t recvBytes = 0;
	int32_t sendBytes = 0;
};

int main()
{

	WSAData wsaData;
	if (::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "start up 에러" << endl;
		return 0;
	}

	//논블로킹(non-blocking)
	SOCKET listenSocket = ::socket(AF_INET, SOCK_STREAM, 0);
	if (listenSocket == INVALID_SOCKET)
		return 0;

	u_long on = 1;
	if (::ioctlsocket(listenSocket, FIONBIO, &on) == INVALID_SOCKET)
		return 0;

	SOCKADDR_IN serverAddr;
	::memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = ::htonl(INADDR_ANY);
	serverAddr.sin_port = ::htons(5252);

	if (::bind(listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		return 0;

	if (::listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
		return 0;

	cout << "Accept" << endl;

	//Select 모델 = (select 함수가 핵심이 되는)
	//소켓 함수 호출이 성공할 시점을 미리 알 수 있다.
	//수신버퍼에 데이터가 없는데, read한다거나 송신버퍼가 꽉 찼는데 write한다거나
	//- 블로킹 소켓 : 조건이 만족되지 않아서 블로킹되는 상황 예방
	//- 논블로킹 소켓 : 조건이 만족되지 않아서 반복체크하는상황 예방

	//socket set
	//1) 읽기 [] 쓰기 [] 예외[](OOB) 관찰 대상 등록
	//OOB : OutOfBand는 send() 마지막 인자 MSG_OOB로 보내는 특별한 데이터
	//받는 쪽에서도 recv OOB 셋팅을 해야 읽을 수 있음
	//긴급상황 특이한상황인데 잘 쓸일없다.
	//2)select(readSet,writeSet,exceptSet); -> 관찰 시작 필요없는부분은 null로 설정
	//3)적어도 하나의 소켓이 준비되면 리턴 -> 준비되지 않은 소켓은 제거
	//4)남은 소켓 체크해서 진행

	// fd_set read;   fd는 파일디스크립터를 뜻 함
	// FD_ZERO : 비운다
	//ex) FD_ZERO(set);
	//FD_SET : 소켓 s를 넣는다.
	//ex) FD_SET(s,&set);
	//FD_CLR : 소켓 s를 제거
	//ex)FD_CLR(s,&set);
	//FD_ISSET : 소켓 s가 SET에 들어가있으면 0이 아닌 값을 리턴

	vector<Session> sessions;
	sessions.reserve(100);

	fd_set reads;
	fd_set writes;

	while (true)
	{
		//소켓 셋 초기화
		FD_ZERO(&reads);
		FD_ZERO(&writes);

		//ListenSocket 등록
		FD_SET(listenSocket, &reads);

		//소켓 등록
		for (Session& s : sessions)
		{
			if (s.recvBytes <= s.sendBytes)
				FD_SET(s.socket, &reads);
			else
				FD_SET(s.socket, &writes);
		}

		//4번쨰 인자의 경우 [옵션] 마지막 timeout 인자 설정 가능
		//defalut : select 가 하나라도 소켓이 준비될떄까지 무한대기
		int32_t retVal = ::select(0, &reads, &writes, nullptr, nullptr);
		if (retVal == SOCKET_ERROR)
			break;

		//Listener 소켓 체크
		if (FD_ISSET(listenSocket, &reads))
		{
			SOCKADDR_IN clientAddr;
			int32_t addrLen = sizeof(clientAddr);
			SOCKET clientSocket = ::accept(listenSocket, (SOCKADDR*)&clientAddr, &addrLen);
			if (clientSocket != INVALID_SOCKET)
			{
				cout << "Client Connected" << endl;
				sessions.push_back(Session{ clientSocket });
			}
		}

		//나머지 소켓 체크
		for (Session& s : sessions)
		{
			//Read 체크
			if (FD_ISSET(s.socket, &reads))
			{
				int32_t recvLen = ::recv(s.socket, s.recvBuffer, BUFSIZE, 0);
				if (recvLen <= 0)
				{
					//TODO : 나중에 sessions에서 제거해야함.
					continue;
				}
				s.recvBytes = recvLen;
			}

			//Write
			if (FD_ISSET(s.socket, &writes))
			{
				//블로킹 모드 -> 모두 다 보내지만
				//논블로킹 모드 -> 일부만 보낼 수가 있음(상대방 수신 버퍼 상황에 따라)
				//send의 리턴은 내가보낸 데이터의 크기가 리턴이 된다.
				//왠만해서는 내가 보낼려는 바이트를 다 보내주긴하지만 상대방의 수신버퍼가 얼마남지않아 조금만 보내지는 경우가 있다.
				//위의 경우가 마이크로소프트 공식문서에서는 일단 있다고 함.
				int32_t sendLen = ::send(s.socket, &s.recvBuffer[s.sendBytes], s.recvBytes - s.sendBytes, 0);
				if (sendLen == SOCKET_ERROR)
				{
					//TODO : session 제거
					continue;
				}

				s.sendBytes += sendLen;
				if (s.recvBytes == s.sendBytes)
				{
					s.recvBytes = 0;
					s.sendBytes = 0;
				}
			}
		}
	}


	//윈속 종료
	::WSACleanup();

}

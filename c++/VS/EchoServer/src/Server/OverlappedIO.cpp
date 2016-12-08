#include <WinSock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <thread>
#include <map>

typedef struct SocketInfo
{
	bool send;
	char buffer[1024];
	WSABUF buf;
	DWORD len;
	WSAOVERLAPPED overlapped;
	SOCKET socket;
} SOCKETINFO;
#define EVENTS_NUM 64
static HANDLE events[EVENTS_NUM];
static SOCKETINFO sockets[EVENTS_NUM];

void overlapped_thread()
{
	while (true)
	{
		DWORD index = WSAWaitForMultipleEvents(EVENTS_NUM, events, false, WSA_INFINITE, false);
		int i = index - WSA_WAIT_EVENT_0;
		if (!WSAResetEvent(events[i]))
		{
			printf("reset error\n");
			continue;
		}
		if (!GetOverlappedResult(events[i], &sockets[i].overlapped, &sockets[i].len, false))
		{
			printf("get overlapped result error\n");
			shutdown(sockets[i].socket, SD_BOTH);
			closesocket(sockets[i].socket);
			sockets[i].socket = INVALID_SOCKET;
			continue;
		}
		if (sockets[i].len == 0)
		{
			closesocket(sockets[i].socket);
			sockets[i].socket = INVALID_SOCKET;
			continue;
		}
		if (!sockets[i].send)
		{
			printf("%d: %s\n", sockets[i].socket, sockets[i].buffer);
			sockets[i].send = true;
			WSASend(sockets[i].socket, &sockets[i].buf, 1, &sockets[i].len, 0, &sockets[i].overlapped, NULL);
		}
		else
		{
			DWORD flag = 0;
			sockets[i].send = false;
			WSARecv(sockets[i].socket, &sockets[i].buf, 1, &sockets[i].len, &flag, &sockets[i].overlapped, NULL);
		}
	}
}

void overlappedServer()
{
	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	addrinfo* addr = nullptr;
	addrinfo hint;
	ZeroMemory(&hint, sizeof(hint));
	hint.ai_flags = AI_NUMERICHOST;
	hint.ai_family = PF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;

	int err = getaddrinfo("127.0.0.1", "1982", &hint, &addr);
	if (err != 0)
	{
		printf("get addr info error %d", err);
		return;
	}

	err = bind(s, addr->ai_addr, addr->ai_addrlen);
	if (err != 0)
	{
		printf("bind error %d", err);
		return;
	}

	SOCKET listenSocket = listen(s, 8);
	if (listenSocket == INVALID_SOCKET)
	{
		printf("listen error");
		return;
	}
	std::thread(std::bind(&overlapped_thread)).detach();
	for (int i = 0; i < EVENTS_NUM; ++i)
	{
		events[i] = WSACreateEvent();
		sockets[i].buf.buf = sockets[i].buffer;
		sockets[i].buf.len = 1024;
		sockets[i].socket = INVALID_SOCKET;
	}
	while (true)
	{
		SOCKET acceptSocket = accept(s, NULL, NULL);
		if (acceptSocket == INVALID_SOCKET)
		{
			printf("accept error");
			return;
		}

		SocketInfo* pSocket = NULL;
		for (int i = 0; i < EVENTS_NUM; ++i)
		{
			if (sockets[i].socket == INVALID_SOCKET)
			{
				ZeroMemory(&sockets[i].overlapped, sizeof(WSAOVERLAPPED));
				sockets[i].overlapped.hEvent = events[i];
				sockets[i].socket = acceptSocket;
				pSocket = &sockets[i];
				break;
			}
		}
		if (pSocket == NULL)
		{
			shutdown(acceptSocket, SD_BOTH);
			closesocket(acceptSocket);
			continue;
		}
		DWORD flag = 0;
		pSocket->send = false;
		WSARecv(acceptSocket, &(pSocket->buf), 1, &(pSocket->len), &flag, &(pSocket->overlapped), NULL);
	}
}
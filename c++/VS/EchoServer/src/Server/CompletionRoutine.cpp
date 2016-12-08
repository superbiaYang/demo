#include <WinSock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <stdio.h>
#include <thread>
#include <set>

typedef struct SocketInfo
{
	char buffer[1024];
	WSABUF buf;
	DWORD len;
	WSAOVERLAPPED overlapped;
	SOCKET socket;
} SOCKETINFO;
#define EVENTS_NUM 64
static HANDLE events[EVENTS_NUM];
static SOCKETINFO sockets[EVENTS_NUM];

void CALLBACK writeCompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);

void CALLBACK readCompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags) {
	for (int i = 0; i < EVENTS_NUM; i++)
	{
		if (events[i] == lpOverlapped->hEvent)
		{
			if (dwError != 0)
			{
				printf("error %d\n", dwError);
				shutdown(sockets[i].socket, SD_BOTH);
				closesocket(sockets[i].socket);
				sockets[i].socket = INVALID_SOCKET;
				return;
			}
			ZeroMemory(&sockets[i].overlapped, sizeof(WSAOVERLAPPED));
			sockets[i].overlapped.hEvent = events[i];
			printf("%d: %s\n", sockets[i].socket, sockets[i].buffer);
			WSASend(sockets[i].socket, &sockets[i].buf, 1, &sockets[i].len, 0, &sockets[i].overlapped, writeCompletionRoutine);
			return;
		}
	}
}

void CALLBACK writeCompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags) {
	for (int i = 0; i < EVENTS_NUM; i++)
	{
		if (events[i] == lpOverlapped->hEvent)
		{
			if (dwError != 0)
			{
				printf("error %d\n", dwError);
				shutdown(sockets[i].socket, SD_BOTH);
				closesocket(sockets[i].socket);
				sockets[i].socket = INVALID_SOCKET;
				return;
			}
			printf("%d:send %s\n", sockets[i].socket, sockets[i].buffer);
			ZeroMemory(&sockets[i].overlapped, sizeof(WSAOVERLAPPED));
			sockets[i].overlapped.hEvent = events[i];
			DWORD flags = 0;
			int ret = WSARecv(sockets[i].socket, &sockets[i].buf, 1, &sockets[i].len, &flags, &sockets[i].overlapped, readCompletionRoutine);
			return;
		}
	}
}
std::set<SocketInfo*> newSocket;
void completion_routine()
{
	while (true)
	{
		if (!newSocket.empty())
		{
			SocketInfo* pSocket = *newSocket.begin();
			DWORD flag = 0;
			WSARecv(pSocket->socket, &(pSocket->buf), 1, &(pSocket->len), &flag, &(pSocket->overlapped), readCompletionRoutine);
			newSocket.erase(pSocket);
			break;
		}
	}
	while (true)
	{
		if (newSocket.empty())
		{
			DWORD index = WSAWaitForMultipleEvents(EVENTS_NUM, events, false, WSA_INFINITE, true);
		}
		else
		{
			SocketInfo* pSocket = *newSocket.begin();
			DWORD flag = 0;
			WSARecv(pSocket->socket, &(pSocket->buf), 1, &(pSocket->len), &flag, &(pSocket->overlapped), readCompletionRoutine);
			newSocket.erase(pSocket);
		}
	}
}

void completionRoutineServer()
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
	for (int i = 0; i < EVENTS_NUM; ++i)
	{
		events[i] = WSACreateEvent();
		sockets[i].buf.buf = sockets[i].buffer;
		sockets[i].buf.len = 1024;
		sockets[i].socket = INVALID_SOCKET;
	}
	std::thread(std::bind(&completion_routine)).detach();
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
		newSocket.insert(pSocket);
	}
}
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
	bool read;
} SOCKETINFO;
#define EVENTS_NUM 64
static HANDLE events[EVENTS_NUM];
static SOCKETINFO sockets[EVENTS_NUM];

HANDLE IOCP;
void iocp_thread()
{
	while (true)
	{
		DWORD num;
		SocketInfo* info;
		LPOVERLAPPED overlapped;
		if (GetQueuedCompletionStatus(IOCP, &num, (PULONG_PTR)&info, (LPOVERLAPPED *)&overlapped, INFINITE) )
		if (info->read)
		{
			ZeroMemory(&info->overlapped, sizeof(WSAOVERLAPPED));
			printf("%d: %s\n", info->socket, info->buffer);
			WSASend(info->socket, &info->buf, 1, &info->len, 0, &info->overlapped, NULL);
			info->read = false;
		}
		else
		{
			ZeroMemory(&info->overlapped, sizeof(WSAOVERLAPPED));
			printf("%d: %s\n", info->socket, info->buffer);
			DWORD flag = 0;
			WSARecv(info->socket, &info->buf, 1, &info->len, &flag, &info->overlapped, NULL);
			info->read = true;
		}
	}
}

void IOCPServer()
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
	std::thread(std::bind(&iocp_thread)).detach();
	IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
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
		pSocket->read = true;
		CreateIoCompletionPort((HANDLE)acceptSocket, IOCP, (ULONG_PTR)pSocket, 0);
		DWORD flag = 0;
		WSARecv(pSocket->socket, &pSocket->buf, 1, &pSocket->len, &flag, &pSocket->overlapped, NULL);
	}
}
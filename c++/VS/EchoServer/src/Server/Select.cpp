#include <WinSock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <thread>
#include <set>

SOCKET sockets[FD_SETSIZE];

void select_thread()
{
	while (true)
	{
		fd_set recv_set;
		FD_ZERO(&recv_set);
		for (int i = 0; i < FD_SETSIZE; ++i)
		{
			if (sockets[i] != INVALID_SOCKET)
			{
				FD_SET(sockets[i], &recv_set);
			}
		}

		int ret = select(0, &recv_set, NULL, NULL, NULL);
		if (ret == SOCKET_ERROR)
		{
			//printf("select error");
			continue;
		}
		else if (ret > 0)
		{
			for (int i = 0; i < FD_SETSIZE; ++i)
			{
				SOCKET s = sockets[i];
				if (s == INVALID_SOCKET)
				{
					continue;
				}
				if (FD_ISSET(s, &recv_set))
				{
					char buf[1024] = { 0 };
					int len = recv(s, buf, 1024, 0);
					int err = GetLastError();
					if (err == 0)
					{
						printf("%d: %s\n", s, buf);
						send(s, buf, 1024, 0);
						err = GetLastError();
						if (err != 0)
						{
							shutdown(s, SD_BOTH);
							closesocket(s);
							sockets[i] = INVALID_SOCKET;
							printf("%d: send err %d\n", s, err);
							continue;
						}
					}
					else
					{
						shutdown(s, SD_BOTH);
						closesocket(s);
						sockets[i] = INVALID_SOCKET;
						printf("%d: recieve err %d\n", s, err);
						continue;
					}
				}
			}
		}
	}
}

void selectServer()
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
	std::thread(std::bind(&select_thread)).detach();
	for (int i = 0; i < FD_SETSIZE; ++i)
	{
		sockets[i] = INVALID_SOCKET;
	}
	while (true)
	{
		SOCKET acceptSocket = accept(s, NULL, NULL);
		if (acceptSocket == INVALID_SOCKET)
		{
			printf("accept error");
			return;
		}
		for (int i = 0; i < FD_SETSIZE; ++i)
		{
			if (sockets[i] == INVALID_SOCKET)
			{
				sockets[i] = acceptSocket;
				break;
			}
		}
	}	
}
#include <WinSock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <thread>

void thread_task(SOCKET s)
{
	char buf[1024];
	int err;
	while (true)
	{
		memset(buf, 0, sizeof(char) * 1024);
		int len = recv(s, buf, 1024, 0);
		err = GetLastError();
		if (err == 0)
		{
			printf("%d: %s\n", std::this_thread::get_id(), buf);
			send(s, buf, 1024, 0);
			err = GetLastError();
			if (err != 0)
			{
				printf("%d: send err %d\n", std::this_thread::get_id(), err);
				return;
			}
		}
		else
		{
			printf("%d: recieve err %d\n", std::this_thread::get_id(), err);
			return;
		}
	}
	
}

void blockingServer()
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

	while (true)
	{
		SOCKET acceptSocket = accept(s, NULL, NULL);
		if (acceptSocket == INVALID_SOCKET)
		{
			printf("accept error");
			return;
		}
		std::thread(std::bind(&thread_task, acceptSocket)).detach();
	}
}
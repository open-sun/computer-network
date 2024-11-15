#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<string.h>
#include <time.h>
using namespace std;
#define BUFFER_SIZE 1024
DWORD WINAPI handlerRequest(LPVOID lparam)
{
	SOCKET ClientSocket = (SOCKET)(LPVOID)lparam;
	char recvBuf[BUFFER_SIZE]={};
	while (1)
	{
		int recresult = recv(ClientSocket, recvBuf, BUFFER_SIZE-8, 0);
		if (recresult > 0)
		{
			recvBuf[recresult] = '\0'; 
			cout << recvBuf << endl;
		}
		else if (recresult == 0)
		{
			cout << "聊天室已关闭" << endl;
			break;
		}
		else
		{
			cout << "聊天室已被关闭" << endl;
			break;
		}
	}
	closesocket(ClientSocket);
	return 0;
}

int main()
{
	int result = 0;
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	result=WSAStartup(wVersionRequested, &wsaData);
	if (result != 0)
	{
		cout << "出错了";
	}
	else
	{
		cout << "聊天室加载中"<<endl;
	}
	SOCKET sockClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in addrSrv;
	addrSrv.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &addrSrv.sin_addr); // 服务器 IP 地址
	addrSrv.sin_port = htons(8088);
	result = connect(sockClient, (SOCKADDR*)&addrSrv, sizeof(addrSrv));
	if (result == SOCKET_ERROR)
	{
		cout << "出错了: 连接失败，错误码: " << WSAGetLastError() << endl;
		closesocket(sockClient);
		WSACleanup();
		return 1;
	}
	cout << "连接成功，让我们开始聊天吧" << endl;
	HANDLE hThread = CreateThread(NULL, NULL, handlerRequest, LPVOID(sockClient), 0, 0);
	char sendbuf[BUFFER_SIZE] = {};

	while (1)
	{
		cin.getline(sendbuf, BUFFER_SIZE);
		if (strcmp(sendbuf, "exit") == 0)
		{
			send(sockClient, sendbuf, strlen(sendbuf), 0);
			break;
		}
		int sendresult = send(sockClient, sendbuf, strlen(sendbuf), 0);
		if (sendresult == SOCKET_ERROR)
		{
			cout << "出错了: 发送数据失败，错误码: " << WSAGetLastError() << endl;
			break;
		}
	}
	closesocket(sockClient);
	WSACleanup();
	return 0;
}

#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<string.h>
#include<vector>
#include <time.h>
using namespace std;
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 3  
vector<SOCKET> clientSockets;
DWORD WINAPI handlerRequest(LPVOID lparam)
{
	SOCKET ClientSocket = (SOCKET)(LPVOID)lparam;
	char sendBuf1[BUFFER_SIZE] = {};
	time_t current_timestamp = time(NULL);
	struct tm local_time;
	localtime_s(&local_time, &current_timestamp);
	char buffer[26];
	asctime_s(buffer, sizeof(buffer), &local_time);
	snprintf(sendBuf1, BUFFER_SIZE, "欢迎[用户%u]加入聊天室  %s", GetCurrentThreadId(), buffer);
	int sendResult = send(ClientSocket, sendBuf1, strlen(sendBuf1), 0);
	if (sendResult == SOCKET_ERROR) {
		printf("Send failed with error: %d\n", WSAGetLastError());
		closesocket(ClientSocket);
		return 1;
	}
	cout << sendBuf1<<endl;
		
	while (1)
	{
		char sendBuf[BUFFER_SIZE] = {};
		char recvBuf[BUFFER_SIZE] = {};
		int recvResult = recv(ClientSocket, recvBuf, BUFFER_SIZE - 8, 0);
		if (recvResult == SOCKET_ERROR) {
			printf("[用户%u]  %s  %s", GetCurrentThreadId(), "已退出",buffer);
			snprintf(sendBuf, BUFFER_SIZE, "[用户%u]  %s  %s", GetCurrentThreadId(), "已退出", buffer);
			for (auto& sock : clientSockets)
			{
				if (sock != ClientSocket) // 不向发送者本身发送
				{
					int sresult = send(sock, sendBuf, strlen(sendBuf), 0);
					if (sresult == SOCKET_ERROR) {
						printf("Send failed with error: %d\n", WSAGetLastError());
						continue; 
					}
				}
			}
			clientSockets.erase(remove(clientSockets.begin(), clientSockets.end(), ClientSocket), clientSockets.end());
			break;
		}
		recvBuf[recvResult] = '\0';
		if (strcmp(recvBuf, "exit") == 0)
		{
			printf("[用户%u]  %s  %s", GetCurrentThreadId(), "已退出", buffer);
			snprintf(sendBuf, BUFFER_SIZE, "[用户%u]  %s  %s", GetCurrentThreadId(), "已退出", buffer);
			for (auto& sock : clientSockets)
			{
				if (sock != ClientSocket) 
				{
					int sresult = send(sock, sendBuf, strlen(sendBuf), 0);
					if (sresult == SOCKET_ERROR) {
						printf("Send failed with error: %d\n", WSAGetLastError());
						continue; 
					}
				}
			}
			clientSockets.erase(remove(clientSockets.begin(), clientSockets.end(), ClientSocket), clientSockets.end());
			break;
		}
		snprintf(sendBuf, BUFFER_SIZE, "[用户%u]  %s  %s", GetCurrentThreadId(), recvBuf,buffer);
		cout << sendBuf<<endl;
		if (recvResult > 0)
		{
			for (auto& sock : clientSockets)
			{
				if (sock != ClientSocket) 
				{
					int sresult = send(sock, sendBuf, strlen(sendBuf), 0);
					if (sresult == SOCKET_ERROR) {
						printf("Send failed with error: %d\n", WSAGetLastError());
						continue; 
					}
				}
			}
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
		cout << "聊天室is加载中。。。。"<<endl;
	}

	SOCKET sockSrv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockSrv == INVALID_SOCKET)
	{
		cout << "出错了" << ",错误请看" << WSAGetLastError();
		WSACleanup();
	}
	sockaddr_in addrSrv;
	addrSrv.sin_family = AF_INET;
	const char* ip = "127.0.0.1";
	if (inet_pton(AF_INET, ip, &addrSrv.sin_addr) <= 0) {
		std::cerr << "inet_pton失败了 " << WSAGetLastError() << std::endl;
		closesocket(sockSrv);
		WSACleanup();
		return 1;
	}
	addrSrv.sin_port = htons(8088);
	bind(sockSrv, (SOCKADDR*)&addrSrv, sizeof(SOCKADDR));
	sockaddr_in addrClient;
	listen(sockSrv, 3);
	while (1) {
		DWORD dwThreadId;
		int len = sizeof(addrClient);
		SOCKET sockConn = accept(sockSrv, (SOCKADDR*)&addrClient, &len);
		if (clientSockets.size() >= MAX_CLIENTS) {
			char rejectMsg[BUFFER_SIZE] = "聊天室已满，无法连接。";
			send(sockConn, rejectMsg, strlen(rejectMsg), 0);
			closesocket(sockConn);
			continue;
		}
		clientSockets.push_back(sockConn);
		HANDLE hThread = CreateThread(NULL, NULL, handlerRequest, LPVOID(sockConn), 0, &dwThreadId);
		CloseHandle(hThread);
	}
	closesocket(sockSrv);
	WSACleanup();
	return 0;
}

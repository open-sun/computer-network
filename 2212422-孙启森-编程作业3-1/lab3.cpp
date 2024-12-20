#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<string.h>
#include <fstream>
#include<vector>
#include <time.h>
#include <ctime>
using namespace std;
#pragma comment(lib, "ws2_32.lib")
#define serverport 8080
#define clientport 8090
#define size 2048
#define SYN 0b01
#define ACK 0b10
#define FIN 0b100
#define PUSH 0b1000
SOCKET serversocket;
sockaddr_in clientaddr;
int seq = 0;
int ack = 0;
double waittime =500;
long datasize = 0;
time_t start_time, end_time;
time_t filestart_time, fileend_time;
struct Header {
	uint16_t sourcePort;       
	uint16_t destinationPort;  
	uint32_t seqnum;   
	uint32_t acknum; 
	uint16_t Flags;         
	uint16_t checksum; 
	uint32_t length;
};
struct message
{
	Header head;
	char data[size];
	message() :head{ 0, 0, 0, 0, 0, 0,0 } {
		memset(data, 0, size);
	}
	void setchecksum();
	bool check();
	void setsyn() { this->head.Flags |= SYN; }
	void setack() { this->head.Flags |= ACK; }
	void setfin() { this->head.Flags |= FIN; }
	void setpush() { this->head.Flags |= PUSH; }
	bool ispush() { return (this->head.Flags & PUSH) != 0; }
	bool issyn(){ return (this->head.Flags & SYN) != 0; }
	bool isack() { return (this->head.Flags & ACK) != 0; }
	bool isfin() { return (this->head.Flags & FIN) != 0; }
	void print() { 
		printf("----------------\n");
		printf("Source Port: %u\n",head.sourcePort);
		printf("Destination Port: %u\n",head.destinationPort);
		printf("Sequence Number: %u\n", head.seqnum);
		printf("Acknowledgment Number: %u\n",head.acknum);
		printf("Flags: ");
		if (issyn()) printf("SYN ");
		if (isfin()) printf("FIN ");
		if (head.Flags & ACK) printf("ACK ");
		if (head.Flags & PUSH) printf("PUSH ");
		printf("\n");
		printf("Checksum: %u\n", head.checksum);
		printf("Length: %u\n",head.length);
		printf("----------------\n");
	}

};
void message::setchecksum()
{
	this->head.checksum = 0;
	uint32_t sum = 0;
	uint16_t* p = (uint16_t*)this;
	for (int i = 0; i < sizeof(*this) / 2; i++)
	{
		sum += *p++;
		while (sum >> 16)
		{
			sum = (sum & 0xffff) + (sum >> 16);
		}
	}
	this->head.checksum = ~(sum & 0xffff);
}
bool message::check()
{
	uint32_t sum = 0;
	uint16_t* p = (uint16_t*)this;
	for (int i = 0; i < sizeof(*this) / 2; i++)
	{
		sum += *p++;
		while (sum >> 16)
		{
			sum = (sum & 0xffff) + (sum >> 16);
		}
	}
	return (sum & 0xffff) == 0xffff;
}
bool initial()
{
	serversocket = socket(AF_INET,SOCK_DGRAM, IPPROTO_UDP);
	if (serversocket == INVALID_SOCKET)
	{
		cout << "出错了" << ",错误请看" << WSAGetLastError();
		WSACleanup();
		return false;
	}
	u_long mode = 1; // 1 表示非阻塞模式
	if (ioctlsocket(serversocket, FIONBIO, &mode) != NO_ERROR) {
		std::cerr << "无法设置非阻塞模式: " << WSAGetLastError() << std::endl;
		closesocket(serversocket);
		WSACleanup();
		return false;
	}
	sockaddr_in addrSrv;
	addrSrv.sin_family = AF_INET;
	const char* ip = "127.0.0.1";
	if (inet_pton(AF_INET, ip, &addrSrv.sin_addr) <= 0) {
		std::cerr << "inet_pton失败了 " << WSAGetLastError() << std::endl;
		closesocket(serversocket);
		WSACleanup();
		return false;
	}
	addrSrv.sin_port = htons(serverport);
	if(bind(serversocket, (SOCKADDR*)&addrSrv, sizeof(addrSrv)) == SOCKET_ERROR)
	{
		return false;
	}


	clientaddr.sin_family = AF_INET;
	clientaddr.sin_port = htons(clientport);
	if (inet_pton(AF_INET, ip, &clientaddr.sin_addr) <= 0) {
		std::cerr << "inet_pton失败了 " << WSAGetLastError() << std::endl;
		closesocket(serversocket);
		WSACleanup();
		return false;
	}
	return true;
}
int send(message &messg)
{
	messg.head.sourcePort = serverport;
	messg.head.destinationPort = clientport;
	messg.setchecksum();
	return sendto(serversocket, (char*)&messg, sizeof(messg), 0, (SOCKADDR*)&clientaddr,sizeof(clientaddr));
}
bool connect()
{
	cout << "开始握手" << endl;
	message msg1;
	msg1.setsyn();
	msg1.head.seqnum = seq;
	int len = sizeof(clientaddr);
	if (send(msg1) == SOCKET_ERROR)
	{
		int error_code = WSAGetLastError();
		printf("sendto failed with error: %d\n", error_code);
		closesocket(serversocket); // 关闭套接字
		WSACleanup(); // 清理 Winsock
		return false;
	}
	cout << "发送第一次握手的报文" << endl;
	msg1.print();
	start_time = clock();
	int i = 0;
	message msg2;
	while (1)
	{
		if (recvfrom(serversocket, (char*)&msg2, sizeof(msg2), 0, (SOCKADDR*)&clientaddr, &len) > 0)
		{
			if (!msg2.check() || msg1.head.seqnum + 1 != msg2.head.acknum || !(msg2.issyn() && msg2.isack()))
			{
				cout << "有错误";
				return false;
			}
			else
			{
				cout << "收到第二次握手的报文，第二次握手成功" << endl;
				msg2.print();

			}
			break;
		}
		if (i > 2)
		{
			cout << "重传过多且失败,终止" << endl;
			i = 0;
			closesocket(serversocket); // 关闭套接字
			WSACleanup(); // 清理 Winsock
			return false;
		}
		end_time = clock();
		double elapsed_time = 1000.0 * (end_time - start_time) / CLOCKS_PER_SEC;
		if (elapsed_time > waittime)
		{
			start_time = clock();
			i++;
			cout << "已超时，现在进行重传" << endl;
			send(msg1);
		}

	}
	message msg3;
	seq = seq + 1;
	ack = msg2.head.seqnum + 1;
	msg3.head.seqnum = msg1.head.seqnum + 1;
	msg3.head.acknum = ack;
	msg3.setack();
	if (send(msg3) == SOCKET_ERROR)
	{
		int error_code = WSAGetLastError();
		printf("sendto failed with error: %d\n", error_code);
		closesocket(serversocket); // 关闭套接字
		WSACleanup(); // 清理 Winsock
		return false;
	}
	cout << "发送第三次握手的报文" << endl;
	msg3.print();
	cout << sizeof(msg3) << endl;
	return true;
}
bool saybye()
{
	message msg1;
	msg1.setfin();
	msg1.head.seqnum = seq;
	seq++;
	int len = sizeof(clientaddr);
	if (send(msg1) == SOCKET_ERROR)
	{
		int error_code = WSAGetLastError();
		printf("sendto failed with error: %d\n", error_code);
		closesocket(serversocket); // 关闭套接字
		WSACleanup(); // 清理 Winsock
		return false;
	}
	cout << "发送第一次挥手的报文" << endl;
	msg1.print();
	start_time = clock();
	int i = 0;
	message msg2;
	while (1)
	{
		if (recvfrom(serversocket, (char*)&msg2, sizeof(msg2), 0, (SOCKADDR*)&clientaddr, &len) > 0)
		{
			cout << msg2.head.acknum << endl;
			if (!(seq == msg2.head.acknum) || !msg2.isack() ||!msg2.check())
			{
				cout << "有错误";
				return false;
			}
			cout << "收到第二次挥手的报文" << endl;
			msg2.print();
			break;

		}
		if (i > 2)
		{
			cout << "重传过多且失败,终止" << endl;
			i = 0;
			closesocket(serversocket); // 关闭套接字
			WSACleanup(); // 清理 Winsock
			return false;
		}
		end_time = clock();
		double elapsed_time = 1000.0 * (end_time - start_time) / CLOCKS_PER_SEC;
		if (elapsed_time >waittime)
		{
			start_time = clock();
			i++;
			cout << "已超时，现在进行重传" << endl;
			send(msg1);
		}
	}
	ack++;
	message msg4;
	start_time = clock();
	while (1)
	{
		if (recvfrom(serversocket, (char*)&msg4, sizeof(msg4), 0, (SOCKADDR*)&clientaddr, &len) > 0)
		{
			if (!(seq == msg2.head.acknum) || !msg4.isack() || !msg4.check() || !msg4.isfin())
			{
				cout << "有错误";
				return false;
			}
			cout << "收到第三次挥手的报文" << endl;
			msg4.print();
			break;
		}
		end_time = clock();
		double elapsed_time = 1000.0 * (end_time - start_time) / CLOCKS_PER_SEC;
		if (elapsed_time > waittime)
		{
			cout << "等待时间过长，自动关闭" << endl;
			closesocket(serversocket); // 关闭套接字
			WSACleanup(); // 清理 Winsock
			return true;
		}

	}
	ack++;
	message msg3;;
	msg3.head.seqnum = seq;
	msg3.head.acknum = ack;
	msg3.setack();
	if (send(msg3) == SOCKET_ERROR)
	{
		int error_code = WSAGetLastError();
		printf("sendto failed with error: %d\n", error_code);
	}
	cout << "发送第四次挥手的报文" << endl;
	msg3.print();
	start_time = clock();
	while (1)
	{
		if (recvfrom(serversocket, (char*)&msg4, sizeof(msg4), 0, (SOCKADDR*)&clientaddr, &len) > 0)
		{
			cout << "挥手失败" << endl;
			break;
		}
		end_time = clock();
		double elapsed_time = 1000.0 * (end_time - start_time) / CLOCKS_PER_SEC;
		if (elapsed_time >waittime)
		{
			cout << "等待时间已过，断开连接，挥手成功" << endl;
			closesocket(serversocket); // 关闭套接字
			WSACleanup(); // 清理 Winsock
			return true;
		}

	}
	return true;
}
bool sendfile(char name[size])
{
	int len = sizeof(clientaddr);
	message file;
	FILE* file1 = fopen(name, "rb");
	if (!file1) {
		perror("无法打开文件");
		return false ;
	}

	fseek(file1, 0, SEEK_END);
	long fileSize = ftell(file1);
	datasize = fileSize;
	fseek(file1, 0, SEEK_SET);
	strncpy(file.data, name, strlen(name));
	file.head.seqnum = seq;
	file.head.length =fileSize;
	file.setpush();
	file.head.acknum = ack;
	seq +=1;
	send(file);
	file.print();
	start_time = clock();
	int i = 0;
	while (1)
	{
		message recmessgae;
		if (recvfrom(serversocket, (char*)&recmessgae, sizeof(recmessgae), 0, (SOCKADDR*)&clientaddr, &len) > 0)
		{
			if (!recmessgae.isack() || !recmessgae.check() || recmessgae.head.acknum != seq)
			{
				cout << "接收错误" << endl;
				break;

			}
			break;
		}
		if (i > 5)
		{
			cout << "重传过多且失败,终止" << endl;
			i = 0;
			closesocket(serversocket); // 关闭套接字
			WSACleanup(); // 清理 Winsock
			return false;
		}
		end_time = clock();
		double elapsed_time= 1000.0 * (end_time - start_time) / CLOCKS_PER_SEC;
		if (elapsed_time > waittime)
		{
			start_time = clock();
			i++;
			cout << "已超时，现在进行重传" << endl;
			send(file);
			file.print();
		}
		
	}
	int chunkSize = size;
	int sentBytes = 0;
	char buffer[size];
	int flag = 0;
	while (sentBytes < fileSize) {
		int readBytes = fread(buffer, 1, chunkSize, file1);
		message msg;
		msg.head.sourcePort = serverport;
		msg.head.destinationPort = clientport;
		msg.head.seqnum = seq; 
		msg.head.acknum = ack; 
		msg.head.length = readBytes; 
		memcpy(msg.data, buffer, readBytes);
		msg.setchecksum();
		start_time = clock();
		if (sendto(serversocket, (char*)&msg, sizeof(Header) +size, 0, (SOCKADDR*)&clientaddr, sizeof(clientaddr)) == SOCKET_ERROR) {
			int error_code = WSAGetLastError();
			printf("sendto failed with error: %d\n", error_code);
			fclose(file1);
			return false ;
		}
		msg.print();
		sentBytes += readBytes;
		seq += readBytes;
		message recmsg;
		while (1)
		{
			int len = sizeof(clientaddr);
			if (recvfrom(serversocket, (char*)&recmsg, sizeof(recmsg), 0, (SOCKADDR*)&clientaddr, &len) > 0)
			{
				if (i != 0)
				cout << "重传成功" << endl;
				i = 0;
				if (!recmsg.check() || !recmsg.isack()||recmsg.head.acknum!=seq)
				{
					cout << "有错误" << endl;
					flag = 1;
					break;
				}
				break;
			}
			else
			{
				if (i > 2)
				{
					cout << "重传过多且失败,终止" << endl;
					closesocket(serversocket); // 关闭套接字
					WSACleanup(); // 清理 Winsock
					return false;
				}
				end_time = clock();
				double elapsed_time = 1000.0 * (end_time - start_time) / CLOCKS_PER_SEC;
				if (elapsed_time > waittime)
				{
					start_time = clock();
					i++;
					cout << "已超时，现在进行重传" << endl;
					sendto(serversocket, (char*)&msg, sizeof(Header) + size, 0, (SOCKADDR*)&clientaddr, sizeof(clientaddr));
					msg.print();
				}
			}

		}
		if (flag == 1)
			break;
	}
	cout << "文件传输成功" << endl;
	fclose(file1);
	return true;
}
int main()
{
	int result = 0;
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	result = WSAStartup(wVersionRequested, &wsaData);
	if (result != 0)
	{
		cout << "出错了";
	}
	if (!initial())
	{
		cout << "创建套接字失败" << endl;
	}
	if (connect())
	{
		cout << "---------------" << endl << "开始传输" << endl;
		char name[size] = {};
		while (1)
		{
			cout << "扣一传文件，扣2断开连接" << endl;
			int kk = 0;
			cin >> kk;
			if (kk == 1)
			{
				cout << "请输入你要传送的文件" << endl;
				cin >> name;
				filestart_time = clock();
				if (sendfile(name))
				{
					fileend_time = clock();
					double chuanshu = 1000.0 * (fileend_time - filestart_time) / CLOCKS_PER_SEC;
					cout << "传输用时" << chuanshu << "ms" << endl;
					cout << "传输文件大小" << datasize << "字节" << endl;
					cout << "吞吐率为" << datasize / chuanshu << "字节/ms" << endl;
				}
				else
				{
					cout << "传输失败" << endl;
				}
			}
			else if (kk == 2)
			{
				saybye();
				break;
			}

		}

	}
	else
	{
		cout << "握手失败,请重试" << endl;
	}
	return 0;
}
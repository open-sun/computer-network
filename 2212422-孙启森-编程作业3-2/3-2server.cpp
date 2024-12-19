#define _CRT_SECURE_NO_WARNINGS
#include<iostream>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<string.h>
#include<vector>
#include <time.h>
#include <thread>  // For std::this_thread
#include <chrono>  //
#include <fstream>
using namespace std;
#pragma comment(lib, "ws2_32.lib")
#define serverport 8090
#define clientport 8080
#define size 2048
#define SYN 0b01
#define ACK 0b10
#define FIN 0b100
#define PUSH 0b1000
SOCKET serversocket;
sockaddr_in clientaddr;
int seq = 0;
int ack = 0;
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
	void setfin() { this->head.Flags |= FIN; }
	void setack() { this->head.Flags |= ACK; }
	void setpush() { this->head.Flags |= PUSH; }
	bool ispush() { return (this->head.Flags & PUSH) != 0; }
	bool issyn() { return (this->head.Flags & SYN) != 0; }
	bool isack() { return (this->head.Flags & ACK) != 0; }
	bool isfin() { return (this->head.Flags & FIN) != 0; }
	void print() {
		printf("----------------\n");
		printf("Source Port: %u\n", head.sourcePort);
		printf("Destination Port: %u\n", head.destinationPort);
		printf("Sequence Number: %u\n", head.seqnum);
		printf("Acknowledgment Number: %u\n", head.acknum);
		printf("Flags: ");
		if (issyn()) printf("SYN ");
		if (isfin()) printf("FIN ");
		if (head.Flags & ACK) printf("ACK ");
		if (head.Flags & PUSH) printf("PUSH ");
		printf("\n");
		printf("Checksum: %u\n", head.checksum);
		printf("Length: %u\n", head.length);
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
	if (!((sum & 0xffff) == 0xffff))
	{
		cout << sum << endl;
	}
	return (sum & 0xffff) == 0xffff;
}
bool initial()
{
	serversocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (serversocket == INVALID_SOCKET)
	{
		cout << "出错了" << ",错误请看" << WSAGetLastError();
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
	if (bind(serversocket, (SOCKADDR*)&addrSrv, sizeof(addrSrv)) == SOCKET_ERROR)
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
int send(message& messg)
{
	messg.head.sourcePort = serverport;
	messg.head.destinationPort = clientport;
	messg.setchecksum();
	return sendto(serversocket, (char*)&messg, sizeof(messg), 0, (SOCKADDR*)&clientaddr, sizeof(clientaddr));
}
bool connect()
{

	message msg[3];
	int len = sizeof(clientaddr);
	while (1)
	{
		if (recvfrom(serversocket, (char*)&msg[0], sizeof(msg[0]), 0, (SOCKADDR*)&clientaddr, &len) > 0)
		{
			if (!msg[0].check())
			{
				cout << "校验失败" << endl;
			}
			break;
		}
	}
	cout << "第一次握手成功" << endl;
	msg[0].print();
	cout << "发送第二次握手的报文" << endl;
	msg[1].setsyn();
	msg[1].setack();
	ack = msg[0].head.seqnum + 1;
	msg[1].head.acknum = ack;
	if (send(msg[1]) == SOCKET_ERROR)
	{
		int error_code = WSAGetLastError();
		printf("sendto failed with error: %d\n", error_code);
	}
	msg[1].print();

	seq += 1;
	while (1)
	{
		if (recvfrom(serversocket, (char*)&msg[2], sizeof(msg[2]), 0, (SOCKADDR*)&clientaddr, &len) > 0)
		{
			if (msg[2].check() && msg[2].isack() && msg[2].head.acknum == seq)
			{
				cout << "收到第三次握手的报文" << endl;
				msg[2].print();
				cout << "握手成功" << endl;
				return true;
				break;
			}
			else
			{
				cout << "握手失败" << endl;
				return false;
			}

		}
	}

	return true;
}
bool saybye()
{

	message msg[3];
	int len = sizeof(clientaddr);
	msg[0].setack();
	msg[0].head.seqnum = seq;
	msg[0].head.acknum = ack;
	if (send(msg[0]) == SOCKET_ERROR)
	{
		int error_code = WSAGetLastError();
		printf("sendto failed with error: %d\n", error_code);
	}
	cout << "发送第二次挥手的报文" << endl;
	msg[0].print();
	seq++;
	msg[1].setack();
	msg[1].setfin();
	msg[1].head.acknum = ack;
	msg[1].head.seqnum = seq;
	send(msg[1]);
	cout << "发送第三次挥手的报文" << endl;
	msg[1].print();
	seq++;
	while (1)
	{
		recvfrom(serversocket, (char*)&msg[2], sizeof(msg[2]), 0, (SOCKADDR*)&clientaddr, &len);
		if (!msg[2].check() || !msg[2].isack() || !(msg[2].head.acknum == seq))
		{
			cout << "校验错误" << endl;
			return false;
		}
		break;
	}
	cout << "收到第四次挥手的报文" << endl;
	msg[2].print();
	cout << "终于说再见" << endl;

	return true;
}
void recfile()
{
	int len = sizeof(clientaddr);
	message msg;
	message sendmsg;
	ofstream outFile;
	long recived = 0;
	long filesize = 0;
	int i = 0;
	while (true)
	{

		recvfrom(serversocket, (char*)&msg, sizeof(msg), 0, (SOCKADDR*)&clientaddr, &len);
		if (msg.ispush() && msg.check() && msg.head.acknum == seq && msg.head.seqnum == ack)
		{
			recived = 0;
			if (outFile.is_open()) {
				outFile.close();
			}

			outFile.open(msg.data, std::ios::out | std::ios::binary);
			if (!outFile) {
				perror("无法打开文件");
				break;
			}
			cout << "new file";
			filesize = msg.head.length;
			cout << filesize << endl;
			recived++;
			ack += 1;
			sendmsg.setack();
			sendmsg.head.acknum = ack;
			sendmsg.head.seqnum = seq;
			send(sendmsg);
			sendmsg.print();
			cout << "wat for" << ack << endl;

		}
		else if (msg.check() && msg.head.acknum == seq && (recived +1) < filesize && msg.head.seqnum == ack)
		{
			ack += 1;
			recived++;
			sendmsg.head.acknum = ack;
			sendmsg.head.seqnum = seq;
			sendmsg.setack();
			send(sendmsg);
			if (outFile.is_open())
			{
				outFile.write(msg.data, msg.head.length);
			}
			sendmsg.print();
			cout << "wat for" << ack << endl;
		}
		else if (msg.check() && msg.head.acknum == seq && (recived +1) >= filesize && msg.head.seqnum == ack)
		{
			recived ++;
			ack += 1;
			sendmsg.head.acknum = ack;
			sendmsg.head.seqnum = seq;
			sendmsg.setack();
			send(sendmsg);
			sendmsg.print();
			if (outFile.is_open())
			{
				outFile.write(msg.data, msg.head.length);
			}
			outFile.close();
			cout << "传输文件成功" << endl;
		}
		else if (msg.check() && msg.head.acknum == seq && msg.head.seqnum != ack)
		{
			send(sendmsg);
		}
		else if (msg.check() && msg.isfin())
		{
			ack += 1;
			cout << "收到第一次挥手的报文" << endl;
			msg.print();
			if (!saybye())
			{
				cout << "没能说再见" << endl;
			}
			else
			{
				cout << "bye bye" << endl;
				closesocket(serversocket);
				WSACleanup();
				break;
			}
		}
	}

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
		cout << "---------------" << endl << "开始传输,进行接收" << endl;
		recfile();

	}


	//ofstream outFile("1.jpg", ios::out | ios::binary);
	//if (!outFile) {
	//	perror("无法打开文件");
	//	return false;
	//}

	//int len = sizeof(clientaddr);
	//message msg;
	//vector<char> receivedData;
	//uint32_t lastSeqNum = 0;
	//message sendmsg;
	//sendmsg.head.seqnum = 0;
	//while (true)
	//{
	//	int bytesReceived = recvfrom(serversocket, (char*)&msg, sizeof(msg), 0, (SOCKADDR*)&clientaddr, &len);
	//	msg.print();
	//	if (bytesReceived > 0)
	//	{
	//		if (msg.check() && msg.ispush())
	//		{
	//			outFile.write(msg.data, msg.head.length);
	//			lastSeqNum = msg.head.seqnum;
	//		}
	//		else
	//		{
	//			cout << "收到无效的数据包" << endl;
	//			continue;
	//			break;
	//		}
	//	}
	//	else
	//	{
	//		// 如果没有更多的数据包到达，或者发生错误
	//		break;
	//	}
	//	sendmsg.setack();
	//	sendmsg.head.seqnum++;
	//	sendmsg.head.acknum = msg.head.seqnum+msg.head.length;
	//	send(sendmsg);
	//}

	//outFile.close();
	return 0;
}
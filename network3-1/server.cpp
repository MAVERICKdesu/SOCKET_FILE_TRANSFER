#include <Ws2tcpip.h>
#include<stdio.h>
#include<tchar.h>
#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include <ctime>
#define RTO 10
#define NAME "test.png"
#define PACKSIZE 64
using namespace std;
#pragma comment(lib, "ws2_32.lib")
WSAData wsd;
SOCKET server;
char * pszRecv = NULL; //接收数据的数据缓冲区指针
int nRet = 0;
int dwSendSize = 0;
unsigned long long Time = 0;
SOCKADDR_IN client, local;    //远程发送机地址和本机接收机地址

const char sk1 = 0x01;
const char sk2 = 0x02;
const char sk3 = 0x04;
const char sk1f = 0x11;
const char sk2f = 0x12;
const char sk3f = 0x14;
const char ACK = 0x08;
const char FIN = 0x10;
const char SEND = 0x20;
const char AGAIN = 0x40;
char again[] = { 0x20,'\0','\0' };//重传数据包
char ack[] = { 0x08,'\0','\0' };//重传数据包

char cal_checksum(char* s, int end)
{
	char sum=0;
	for (int i = 0; i < end; ++i)
		sum += s[i];
	return ~sum;
}

bool check_checksum(char*s,int end)
{
	char sum=0;
	int i;
	for (i = 0; i < end-1; ++i)
		sum += s[i];
	if (~sum == s[i])
		return true;
	else
		return false;
}

bool shake_hand()
{
	char* sk2_pack = new char[2];
	sk2_pack[0] = sk2, sk2_pack[1] = cal_checksum(sk2_pack, 1);

	nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
	if (nRet == 2 && check_checksum(pszRecv, 2) && pszRecv[0] == sk1)
	{
		cout << "握手1数据包已接收" << endl;
		sendto(server, sk2_pack, 2, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
		nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
		if (nRet == 2 && check_checksum(pszRecv, 2) && pszRecv[0] == sk3)
			cout << "握手3数据包已接收"<<endl<<"握手连接建立成功，开始接受数据" << endl;
	}
	return true;
}

bool wave_hand()
{
	char* sk2_pack = new char[2];
	sk2_pack[0] = sk2f, sk2_pack[1] = cal_checksum(sk2_pack, 1);

	nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
	if (nRet == 2 && check_checksum(pszRecv, 2) && pszRecv[0] == sk1f)
	{
		cout << "挥手1数据包已接收" << endl;
		sendto(server, sk2_pack, 2, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
		nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
		if (nRet == 2 && check_checksum(pszRecv, 2) && pszRecv[0] == sk3f)
			cout << "挥手3数据包已接收" << endl << "挥手连接建立成功，开始接受数据" << endl;
	}
	return true;
}


static void setnonblocking(int sockfd) {
	unsigned long on = 1;
	if (0 != ioctlsocket(sockfd, FIONBIO, &on))
	{
		/* Handle failure. */
	}
}
int loss = 0;
void recv_file()
{
	//fstream makefile;
	//makefile.open(NAME, ios::binary);
	ofstream fout(NAME, ofstream::binary);
	int pack = 0;
	int lastconfirm = -1;
	int stat = 0, laststat = 0;//0 接受数据包 1 数据包为SEND   2 数据包为 FIN  3 发送ACK数据包 4 申请重传
	while (stat!=100)
	{
		switch (stat)
		{
			case 0:
			{
				nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
				/*int t = Time, flag = 0;
				nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
				while (Time < t + RTO)
				{
					nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
					if (nRet != -1)
					{
						flag = 1;
						break;
					}
				}
				if (flag == 0)
				{
					cout << "超时，申请重传" << endl;
					stat = 4;
				}*/
				if (pszRecv[0] == SEND)stat = 1;
				else if (pszRecv[0] == FIN) stat = 2;
				else stat = 100;
			}; break;
			case 1: 
			{
				if (check_checksum(pszRecv, nRet))
				{
					cout << "校验成功,返回ACK数据包 pack=" << (int)pszRecv[1] << " txtlen=" << (int)pszRecv[2] << endl;
					if ((char)(lastconfirm + 1 % 256) == pszRecv[1])
					{
						for (int i = 3; i < 3 + (int)pszRecv[2]; ++i)
							fout << pszRecv[i];
						lastconfirm++;
						stat = 3;
					}
					else
						stat = 0;
				} 
				else
				{
					cout << "校验失败,申请重传 pack=" << (int)pszRecv[1] << " txtlen=" << (int)pszRecv[2] << endl;
					stat = 4;
				}
			};break;
			case 2: 
			{
				if (check_checksum(pszRecv, nRet))
				{
					cout << "FIN数据包校验成功，传输结束" << endl;
					stat = 100;
				}
			}; break;
			case 3:
			{
				ack[1] = pszRecv[1], ack[2] = cal_checksum(ack, 2);
				sendto(server, ack, 3, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
				stat = 0;
			}; break;
			case 4:
			{
				again[2] = cal_checksum(ack, 2);
				sendto(server, again, 3, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
				stat = 0;
			}; break;
			
		}
			
	}
	fout.close();
}


int init()
{
	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0) {
		cout << "WSAStartup Error = " << WSAGetLastError() << endl;
		return 0;
	}
	server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (server == SOCKET_ERROR) {
		cout << "socket Error = " << WSAGetLastError() << endl;
		return 0;
	}
	int nPort = 5010;
	local.sin_family = AF_INET;
	local.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	local.sin_port = htons(nPort);
	if (bind(server, (SOCKADDR*)&local, sizeof(local)) == SOCKET_ERROR) {
		cout << "bind Error = " << WSAGetLastError() << endl;
		return 0;
	}
	return 1;
}

DWORD WINAPI timer(LPVOID lparam)
{
	while (true)
	{
		Sleep(1);
		Time++;
	}
}

int _tmain(int argc, _TCHAR* argv[])//_tmain,要加＃include <tchar.h>才能用
{
	HANDLE hThread = CreateThread(NULL, NULL, timer, 0, 0, NULL);
	init();
	pszRecv = new char[4096];
	dwSendSize = sizeof(client);
	shake_hand();
	//setnonblocking(server);
	recv_file();
	wave_hand();
	closesocket(server);
	delete[] pszRecv;
	WSACleanup();
	cout << Time;
	system("pause");
	return 0;
}
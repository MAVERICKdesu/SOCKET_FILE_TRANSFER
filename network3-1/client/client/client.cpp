#include <Ws2tcpip.h>
#include<stdio.h>
#include<tchar.h>
#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include<fstream>

#include <Windows.h>
#include<cstring>
#define RTO 20
#define PACKSIZE 64
#define NAME "test.png"
using namespace std;

#pragma comment(lib, "ws2_32.lib")
WSAData wsd;           //初始化信息
SOCKET client;         //发送SOCKET
char * pszRecv = NULL; //接收数据的数据缓冲区指针
int nRet = 0;
int dwSendSize = 0;
int pszSend = 0;
unsigned long long Time;
SOCKADDR_IN server,local;    //远程发送机地址和本机接收机地址
int lentmp = sizeof(local);

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
char final[] = { 0x10,'\0','\0' };//重传数据包

char cal_checksum(char* s, int end)
{
	char sum = 0;
	for (int i = 0; i< end; ++i)
		sum += s[i];
	return ~sum;
}

bool check_checksum(char*s, int end)
{
	char sum = 0;
	int i;
	for (i = 0; i < end - 1; ++i)
		sum += s[i];
	return (~sum == s[i]);
}


bool shake_hand()
{
	char* sk1_pack = new char[2];
	sk1_pack[0] = sk1, sk1_pack[1] = cal_checksum(sk1_pack, 1);
	char* sk3_pack = new char[2];
	sk3_pack[0] = sk3, sk3_pack[1] = cal_checksum(sk3_pack, 1);
	sendto(client, sk1_pack, 2, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	nRet = recvfrom(client, pszRecv, 4096, 0, (SOCKADDR*)&server, &lentmp);
	if (nRet == 2 && check_checksum(pszRecv, 2) && pszRecv[0] == sk2)
	{
		cout << "握手2数据包已接收" << endl;
		sendto(client, sk3_pack, 2, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	}
	return true;
}

bool wave_hand()
{
	char* sk1_pack = new char[2];
	sk1_pack[0] = sk1f, sk1_pack[1] = cal_checksum(sk1_pack, 1);
	char* sk3_pack = new char[2];
	sk3_pack[0] = sk3f, sk3_pack[1] = cal_checksum(sk3_pack, 1);
	sendto(client, sk1_pack, 2, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	nRet = recvfrom(client, pszRecv, 4096, 0, (SOCKADDR*)&server, &lentmp);
	if (nRet == 2 && check_checksum(pszRecv, 2) && pszRecv[0] == sk2f)
	{
		cout << "挥手2数据包已接收" << endl;
		sendto(client, sk3_pack, 2, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	}
	return true;
}

char** split_text(const char* txt, int len)
{
	int pack = len / PACKSIZE;
	if (pack*PACKSIZE != len)
		pack++;
	char**packtxt = new char*[pack+1];
	packtxt[pack] = nullptr;
	for (int i = 0,cntch=0; i < pack; ++i)
	{
		packtxt[i] = new char[PACKSIZE+1];
		for (int cnt = 0; cnt < PACKSIZE; cnt++)
		{
			if (cntch == len)
				packtxt[i][cnt] = '0';
			else
			{
				cntch++;
				packtxt[i][cnt] = txt[PACKSIZE*i + cnt];
			}
		}
		packtxt[i][PACKSIZE] = '\0';
	}
	return packtxt;
}

void send_pack(char* txt, int len,int id)
{
	//Sleep(20);
	if (id%10==0)
		Sleep(RTO+RTO);
	char* pack = new char[len + 5];
	pack[0] = SEND;
	pack[1] = id%256;
	pack[2] = len;
	for (int i = 0; i < len; ++i)
		pack[3 + i] = txt[i];
	pack[3 + len] = cal_checksum(pack, 3+len);
	pack[4 + len] = '\0';
	sendto(client, pack, len+4, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	delete[] pack;
}
char* p = new char[20000000];
void send_file(char* filename)
{
	ifstream fin(filename, ifstream::binary);
	unsigned char t = fin.get();
	int len = 0;
	while (fin) {
		p[len++] = t;
		t = fin.get();
	}
	fin.close();
	cout << len;
	int stat=0,i=0;
	char** packtxt=split_text(p,len);

	while (stat!=100)//0 发送数据包 1 接受数据包  2 发送重传数据包 3 发送FIN数据包 4 ACK 
	{
		switch (stat)
		{
			case 0: 
			{
				int txtlen = ((i + 1)* PACKSIZE > len) ? (len - i * PACKSIZE) : PACKSIZE;
				send_pack(packtxt[i], txtlen, i);
				++i;
				cout << i<<endl;
				stat = 1;
			}; break;
			case 1:
			{
				int t = Time, flag = 0;
				while (Time < t + RTO)
				{
					nRet = recvfrom(client, pszRecv, 4096, 0, (SOCKADDR*)&server, &lentmp);
					if (nRet != -1)
					{
						flag = 1;
						break;
					}
				}
				if (flag == 0 )
				{
					cout << "ACK超时，重新发送数据" << endl;
					--i;
					stat = 0;
				}
				else if(pszRecv[0] == ACK)
					stat = 4;
				else if (pszRecv[0] == AGAIN)
					stat = 0,i=i-1;
			}; break;
			case 3:
			{
				final[2] = cal_checksum(final, 2);
				sendto(client, final, 3, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
				stat = 100;
				break;
			}; break;
			case 4:
			{
				if (nRet == 3 && check_checksum(pszRecv, 3) && (pszRecv[1]+1)%256 ^ i%256 ==0)
				{
					cout << "ACK数据包已接收,id=" << (int)pszRecv[1] << endl;
					if (packtxt[i] == nullptr)
						stat = 3;
					else
						stat = 0;
				}
				else
				{
					cout << "ACK数据包有误，数据重传" <<(int)(pszRecv[1] + 1) % 256<<" "<<i%256<< endl;
					stat = 0;
				}
			}; break;
		}
	}
}

int init()
{
	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0) {
		cout << "WSAStartup Error = " << WSAGetLastError() << endl;
		return 0;
	}
	client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (client == SOCKET_ERROR) {
		cout << "socket Error = " << WSAGetLastError() << endl;
		return 0;
	}
	int nPort = 5010;
	server.sin_family = AF_INET;
	server.sin_port = htons(nPort);
	server.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	pszRecv = new char[4096];
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
static void setnonblocking(int sockfd) {
	unsigned long on = 1;
	if (0 != ioctlsocket(sockfd, FIONBIO, &on))
	{ 
	}
}
int main(int argc, _TCHAR* argv[])
{
	HANDLE hThread = CreateThread(NULL, NULL, timer, 0, 0, NULL);
	init();
	shake_hand();
	char file[] = NAME;
	setnonblocking(client);
	send_file(file);
	wave_hand();
	closesocket(client);
	WSACleanup();

	return 0;
}
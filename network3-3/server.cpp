#include <Ws2tcpip.h>
#include<stdlib.h>
#include<stdio.h>
#include<tchar.h>
#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <Windows.h>
#include <ctime>
#define DELAY 20
#define NAME "test.png"
#define PACKSIZE 64
using namespace std;
#pragma comment(lib, "ws2_32.lib")
WSAData wsd;
HANDLE hMutex1;
SOCKET server;
char * pszRecv = NULL; //接收数据的数据缓冲区指针
int nRet = 0;
int dwSendSize = 0, lastconf = -1;
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
char again[] = { 0x20,'\0','\0','\0','\0' };//重传数据包
char ack[] = { 0x08,'\0','\0','\0','\0' };//重传数据包

char cal_checksum(char* s, int end)
{
	char sum = 0;
	for (int i = 0; i < end; ++i)
		sum += s[i];
	return ~sum;
}

bool check_checksum(char*s, int end)
{
	char sum = 0;
	int i;
	for (i = 0; i < end - 1; ++i)
		sum += s[i];
	if (~sum == s[i])
		return true;
	else
		return false;
}

bool shake_hand()
{
	char* sk2_pack = new char[5];
	sk2_pack[0] = sk2, sk2_pack[4] = cal_checksum(sk2_pack, 4);

	nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
	if (nRet == 5 && check_checksum(pszRecv, 5) && pszRecv[0] == sk1)
	{
		cout << "握手1数据包已接收" << endl;
		sendto(server, sk2_pack, 5, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
		nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
		if (nRet == 5 && check_checksum(pszRecv, 5) && pszRecv[0] == sk3)
			cout << "握手3数据包已接收" << endl << "握手连接建立成功，开始接受数据" << endl;
	}
	return true;
}

bool wave_hand()
{
	char* sk2_pack = new char[5];
	sk2_pack[0] = sk2f, sk2_pack[4] = cal_checksum(sk2_pack,4);

	nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
	if (nRet == 5 && check_checksum(pszRecv, 5) && pszRecv[0] == sk1f)
	{
		cout << "挥手1数据包已接收" << endl;
		sendto(server, sk2_pack, 5, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
		nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
		if (nRet == 5 && check_checksum(pszRecv, 5) && pszRecv[0] == sk3f)
			cout << "挥手3数据包已接收" << endl << "挥手连接建立成功，开始接受数据" << endl;
	}
	return true;
}

char** packtxt;
bool sendstat[20000000];
int lentxt[20000000], big = 0;
ofstream fout(NAME, ofstream::binary);
DWORD WINAPI ackm(PVOID sockClient)
{
	WaitForSingleObject(hMutex1, INFINITE);
	bool l = false;
	int *x = (int *)sockClient;
	int xh = *x;
	if (xh > big) big = xh;
	if (xh == lastconf + 1)
		lastconf++;
	else 
	{
		xh = lastconf;
		l = true;
	}
	ack[1] = xh / 256, ack[2] = xh % 256 , ack[4] = cal_checksum(ack, 4);
	sendto(server, ack, 5, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
	if (!sendstat[xh] && !l)
	{
		packtxt[xh] = new char[nRet + 1];
		lentxt[xh] = (int)pszRecv[3];
		cout << "print pack" << xh << endl;
		for (int i = 4; i < 4 + (int)pszRecv[3]; i++)
			//packtxt[xh][i - 4] = pszRecv[i];
			fout<<pszRecv[i];
		//packtxt[xh][nRet] = '\0';
		sendstat[xh] = true;
	}
	ReleaseMutex(hMutex1);
	return 0;
}

int loss = 1;
void recv_file()
{
	packtxt = new char*[5000000];
	int cnt=0;
	while (true)
	{
		cnt++;
		nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
		if (pszRecv[0] == FIN)
			break;
		if (check_checksum(pszRecv, nRet))
		{
			loss++;
			if (loss % 20 <0)
				continue;
			int xh = ((unsigned char)pszRecv[2]) + ((unsigned char)pszRecv[1]) * 256;
			cout << "校验成功,返回ACK数据包 pack=" << xh << " txtlen=" << (int)pszRecv[3] << endl;
			HANDLE hThread = CreateThread(NULL, NULL, ackm, (PVOID)&xh, 0, NULL);
		}
	}
	cout << "cnt " << cnt << endl;
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
	local.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");//("10.139.152.84");
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
	recv_file();
	cout << "out";
	wave_hand();
	closesocket(server);
	delete[] pszRecv;
	WSACleanup();
	fout.close();
	system("pause");
	return 0;
}
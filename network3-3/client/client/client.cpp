#include <Ws2tcpip.h>
#include<stdlib.h>
#include<stdio.h>
#include<tchar.h>
#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include<fstream>
#include<vector>
#include <Windows.h>
#include<cstring>
#include<queue>
#define DELAY 20
#define minn(x,y) (((x)>(y))?(y):(x))
#define maxn(x,y) (((x)<(y))?(y):(x))
#define PACKSIZE 64
#define NAME "test.png"
using namespace std;

#pragma comment(lib, "ws2_32.lib")
int WINDOWSIZE = 20, RTO=200;
int RTT = 100,SRTT=100;
bool fast = false;
int cwmd=1, ssthresh=10;
HANDLE hMutex1;
WSAData wsd;           //初始化信息
SOCKET client;         //发送SOCKET
char * pszRecv = NULL; //接收数据的数据缓冲区指针
int nRet = 0;
int dwSendSize = 0;
int pszSend = 0,len, pack;
unsigned long long Time;
SOCKADDR_IN server, local;    //远程发送机地址和本机接收机地址
int lentmp = sizeof(local), lasttime = 0;
queue<int> waiting;
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
char final[] = { 0x10,'\0','\0','\0','\0' };

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
	return (~sum == s[i]);
}


bool shake_hand()
{
	char* sk1_pack = new char[5];
	sk1_pack[0] = sk1, sk1_pack[4] = cal_checksum(sk1_pack, 4);
	char* sk3_pack = new char[5];
	sk3_pack[0] = sk3, sk3_pack[4] = cal_checksum(sk3_pack, 4);
	sendto(client, sk1_pack, 5, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	nRet = recvfrom(client, pszRecv, 4096, 0, (SOCKADDR*)&server, &lentmp);
	if (nRet == 5 && check_checksum(pszRecv, 5) && pszRecv[0] == sk2)
	{
		cout << "握手2数据包已接收" << endl;
		sendto(client, sk3_pack, 5, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	}
	return true;
}

bool wave_hand()
{
	char* sk1_pack = new char[5];
	sk1_pack[0] = sk1f, sk1_pack[4] = cal_checksum(sk1_pack, 4);
	char* sk3_pack = new char[5];
	sk3_pack[0] = sk3f, sk3_pack[4] = cal_checksum(sk3_pack,4);
	sendto(client, sk1_pack, 5, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	nRet = recvfrom(client, pszRecv, 4096, 0, (SOCKADDR*)&server, &lentmp);
	if (nRet == 5 && check_checksum(pszRecv, 5) && pszRecv[0] == sk2f)
	{
		cout << "挥手2数据包已接收" << endl;
		sendto(client, sk3_pack, 5, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	}
	return true;
}

char** split_text(const char* txt, int len)
{
	pack = len / PACKSIZE;
	cout << pack;
	if (pack*PACKSIZE != len)
		pack++;
	char**packtxt = new char*[pack + 1];
	packtxt[pack] = nullptr;
	for (int i = 0, cntch = 0; i < pack; ++i)
	{
		packtxt[i] = new char[PACKSIZE + 1];
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

void send_pack(char* txt, int len, int id)
{
	char* pack = new char[len + 6];
	pack[0] = SEND;
	pack[1] = id / 256;
	pack[2] = id % 256;
	pack[3] = len;
	for (int i = 0; i < len; ++i)
		pack[4 + i] = txt[i];
	pack[4 + len] = cal_checksum(pack, 4 + len);
	pack[5 + len] = '\0';
	sendto(client, pack, len + 5, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
	delete[] pack;
}
char* p = new char[20000000];
int sendstat[20000000];
int windowl, windowr;
char** packtxt;
bool out=true;
int sendtime[20000000];
DWORD WINAPI sendMessage(LPVOID lparam)
{
	bool O = true;
	for (int i = 0; i <= windowr; ++i)
		waiting.push(i);
	int cnt = 0;
	while(O)
	{
		WaitForSingleObject(hMutex1, INFINITE);
		for(int i=windowl;i<=windowr && i<pack;++i)
		{
			if (sendstat[i] ==0 && (sendtime[i] ==0 || (sendtime[i] != 0 && (Time-sendtime[i])>RTO)))
			{
				if (sendtime[i] != 0)
				{
					ssthresh = maxn(2,cwmd / 2);
					cwmd = 1;
				}
			sendtime[i] = Time;
			int txtlen = ((i + 1)* PACKSIZE > len) ? (len - i * PACKSIZE) : PACKSIZE;
			_sleep(DELAY);
			send_pack(packtxt[i], txtlen, i);
			cnt++;
			cout << i << " send "<<pack << endl;
			}
		}
		windowr = windowl + minn(WINDOWSIZE,cwmd) - 1;
		if (sendstat[pack-1] == true)
			O = false;
		ReleaseMutex(hMutex1);
	}
	cout << "cnt " << cnt<<endl;
	return 0;
}
int all = 0;
DWORD WINAPI acceptMessage(LPVOID lparam)
{
	while (out)
	{
		nRet = recvfrom(client, pszRecv, 4096, 0, (SOCKADDR*)&server, &lentmp);
		WaitForSingleObject(hMutex1, INFINITE);
		all++;
		if (cwmd > ssthresh)
			cwmd++;
		else
			cwmd *= 2;
		int id = ((unsigned char)pszRecv[2]) + ((unsigned char)pszRecv[1]) * 256;
		RTT = Time/all;
		SRTT = (8 * SRTT + 2 * RTT) / 10;
		RTO = 10 * SRTT;
		cout << id << endl<<" SRTT="<<SRTT<<endl;
		if(id!=-1)
			sendstat[id] ++;
		if (sendstat[id] >= 3)
		{
			cwmd = maxn(2,cwmd/2);
			ssthresh = cwmd;
			cwmd += 3;
			cwmd = ssthresh;
		}
		ReleaseMutex(hMutex1);
	}
	return 0;
}
DWORD WINAPI windowmove(LPVOID lparam)
{
	while (out)
	{
		WaitForSingleObject(hMutex1, INFINITE);
		if (Time - lasttime > SRTT)
		{
			if (cwmd <= ssthresh)
				cwmd *= 2;
			else
				cwmd += 1;
			lasttime = Time;
		}
		windowr = windowl + minn(WINDOWSIZE, cwmd) -1;
		while(sendstat[windowl]!=0)
		{
			windowl++;
			windowr++;
			if (windowl == pack-1)
				out = false;
		}
		ReleaseMutex(hMutex1);
	}
	return 0;
}
void send_file(char* filename)
{
	ifstream fin(filename, ifstream::binary);
	unsigned char t = fin.get();
	while (fin) {
		p[len++] = t;
		t = fin.get();
	}
	fin.close();
	cout << len;
	int  i = 0;
	packtxt = split_text(p, len);
	windowl = 0, windowr = minn(WINDOWSIZE, cwmd) - 1;
	HANDLE hThread = CreateThread(NULL, NULL, acceptMessage, 0, 0, NULL);
	HANDLE hThread2 = CreateThread(NULL, NULL, windowmove, 0, 0, NULL);
	HANDLE hThread3 = CreateThread(NULL, NULL, sendMessage, 0, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);
	WaitForSingleObject(hThread2, INFINITE);
	WaitForSingleObject(hThread3, INFINITE);
	final[2] = cal_checksum(final, 2);
	sendto(client, final, 3, 0, (SOCKADDR*)&server, sizeof(SOCKADDR));
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
int jl[1000000][5],ii=0;
DWORD WINAPI timer(LPVOID lparam)
{
	while (true)
	{
		Sleep(1);
		Time++;
		if (Time % 10 == 0)
			jl[ii][0] = Time, jl[ii][1] = SRTT, jl[ii][2] = cwmd, jl[ii][3] = ssthresh, jl[ii][4]=minn(WINDOWSIZE,cwmd),ii += 1;

	}
}

int main(int argc, _TCHAR* argv[])
{
	hMutex1 = CreateMutex(NULL, FALSE, NULL);
	HANDLE hThread = CreateThread(NULL, NULL, timer, 0, 0, NULL);
	init();
	if (shake_hand())cout << "握手连接建立成功，开始发送数据" << endl;
	char file[] = NAME;
	send_file(file);
	wave_hand();
	closesocket(client);
	cout << "total time:" << Time;
	WSACleanup();
	ofstream fout("data.txt", ofstream::out);
	for(int i=0;i<ii;++i)
		fout << jl[i][0] << " " << jl[i][1] << " " << jl[i][2] << " " << jl[i][3]  << " " << jl[i][4]<<endl;
	fout.close();
	return 0;
}
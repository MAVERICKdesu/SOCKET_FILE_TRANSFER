#include <Ws2tcpip.h>
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
#define RTO 500
#define PACKSIZE 64
#define NAME "test.png"
#define WINDOWSIZE 5
using namespace std;

#pragma comment(lib, "ws2_32.lib")
//������
HANDLE hMutex1;
WSAData wsd;           //��ʼ����Ϣ
SOCKET client;         //����SOCKET
char * pszRecv = NULL; //�������ݵ����ݻ�����ָ��
int nRet = 0;
int dwSendSize = 0;
int pszSend = 0,len, pack;
unsigned long long Time;
SOCKADDR_IN server, local;    //Զ�̷��ͻ���ַ�ͱ������ջ���ַ
int lentmp = sizeof(local);
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
char again[] = { 0x20,'\0','\0','\0','\0' };//�ش����ݰ�
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
		cout << "����2���ݰ��ѽ���" << endl;
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
		cout << "����2���ݰ��ѽ���" << endl;
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
bool sendstat[20000000];
int windowl, windowr;
char** packtxt;
bool out=true;
int sendtime[20000000];
DWORD WINAPI sendMessage(LPVOID lparam)
{
	for (int i = 0; i <= windowr; ++i)
		waiting.push(i);
	int cnt = 0;
	while(out)
	{
		WaitForSingleObject(hMutex1, INFINITE);
		queue<int> temp;
		while(!waiting.empty())
		{
			int i = waiting.front();
			waiting.pop();
			if (sendstat[i] == false && (sendtime[i]==0 || (sendtime[i]!=0 && Time-sendtime[i]>RTO)))
			{
				sendtime[i] = Time;
				int txtlen = ((windowl + 1)* PACKSIZE > len) ? (len - windowl * PACKSIZE) : PACKSIZE;
				send_pack(packtxt[windowl], txtlen, windowl);
				cnt++;
				cout << i << " send" << endl;
			}
			temp.push(i);
		}
		while (!temp.empty())
		{
			int i = temp.front();
			temp.pop();
			waiting.push(i);
		}
		ReleaseMutex(hMutex1);
	}
	cout << "cnt " << cnt;
	return 0;
}
DWORD WINAPI acceptMessage(LPVOID lparam)
{
	while (out)
	{
		nRet = recvfrom(client, pszRecv, 4096, 0, (SOCKADDR*)&server, &lentmp);
		int id= ((unsigned char)pszRecv[2]) +((unsigned char)pszRecv[1])*256;
		cout << id << " " << pack << endl;
		sendstat[id] = true;
	}
	return 0;
}
DWORD WINAPI windowmove(LPVOID lparam)
{
	while (out)
	{
		bool first = true;
		if (sendstat[windowl])
		{
			WaitForSingleObject(hMutex1, INFINITE);
			windowl++;
			windowr++;
			waiting.pop();
			if ((first && windowl == pack - 1) || windowl!=pack-1)
			{
				waiting.push(windowr);
				first = false;
			}
			if (windowl == pack-1)
				out = false;
			ReleaseMutex(hMutex1);
		}
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
	windowl = 0, windowr = WINDOWSIZE - 1;
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

DWORD WINAPI timer(LPVOID lparam)
{
	while (true)
	{
		Sleep(1);
		Time++;
	}
}

int main(int argc, _TCHAR* argv[])
{
	hMutex1 = CreateMutex(NULL, FALSE, NULL);
	HANDLE hThread = CreateThread(NULL, NULL, timer, 0, 0, NULL);
	init();
	if (shake_hand())cout << "�������ӽ����ɹ�����ʼ��������" << endl;
	char file[] = NAME;
	send_file(file);
	wave_hand();
	closesocket(client);
	WSACleanup();

	return 0;
}
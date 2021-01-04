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
char * pszRecv = NULL; //�������ݵ����ݻ�����ָ��
int nRet = 0;
int dwSendSize = 0;
unsigned long long Time = 0;
SOCKADDR_IN client, local;    //Զ�̷��ͻ���ַ�ͱ������ջ���ַ

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
char again[] = { 0x20,'\0','\0' };//�ش����ݰ�
char ack[] = { 0x08,'\0','\0' };//�ش����ݰ�

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
	char* sk2_pack = new char[2];
	sk2_pack[0] = sk2, sk2_pack[1] = cal_checksum(sk2_pack, 1);

	nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
	if (nRet == 2 && check_checksum(pszRecv, 2) && pszRecv[0] == sk1)
	{
		cout << "����1���ݰ��ѽ���" << endl;
		sendto(server, sk2_pack, 2, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
		nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
		if (nRet == 2 && check_checksum(pszRecv, 2) && pszRecv[0] == sk3)
			cout << "����3���ݰ��ѽ���" << endl << "�������ӽ����ɹ�����ʼ��������" << endl;
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
		cout << "����1���ݰ��ѽ���" << endl;
		sendto(server, sk2_pack, 2, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
		nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
		if (nRet == 2 && check_checksum(pszRecv, 2) && pszRecv[0] == sk3f)
			cout << "����3���ݰ��ѽ���" << endl << "�������ӽ����ɹ�����ʼ��������" << endl;
	}
	return true;
}

char** packtxt;
bool sendstat[20000000];
int lastconf = 0;
int lentxt[20000000];

void recv_file()
{
	packtxt = new char*[5000000];
	//fstream makefile;
	//makefile.open(NAME, ios::binary);
	ofstream fout(NAME, ofstream::binary);
	while (true)
	{
		if (sendstat[lastconf])
			lastconf++;
		nRet = recvfrom(server, pszRecv, 4096, 0, (SOCKADDR*)&client, &dwSendSize);
		if (pszRecv[0] == FIN)
			break;
		if (check_checksum(pszRecv, nRet))
		{
			int temp = pszRecv[1];
			if (temp < 0) temp = 256 + temp;
			int temp2 = lastconf % 256;
			cout << "У��ɹ�,����ACK���ݰ� pack=" << (int)pszRecv[1] << " txtlen=" << (int)pszRecv[2] << endl;
			ack[1] = pszRecv[1], ack[2] = cal_checksum(ack, 2);
			sendto(server, ack, 3, 0, (SOCKADDR*)&client, sizeof(SOCKADDR));
			if (!sendstat[lastconf + (256 + temp - temp2) % 256])
			{
				packtxt[lastconf + (256 + temp - temp2) % 256] = new char[nRet+1];
				lentxt[lastconf + (256 + temp - temp2) % 256] = (int)pszRecv[2];
				for (int i = 3; i < 3+ (int)pszRecv[2]; i++)
					packtxt[lastconf + (256 + temp - temp2) % 256][i-3]= pszRecv[i];
				packtxt[lastconf + (256 + temp - temp2) % 256][nRet] = '\0';
				sendstat[lastconf + (256 + temp - temp2) % 256] = true;
			}
		}
	}
	for (int i = 0; i <= lastconf; ++i)
		for (int j = 0; j<lentxt[i]; ++j)
			fout << packtxt[i][j];
		/*switch (stat)
		{
		case 0:
		{
			int t = Time, flag = 0;
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
				cout << "��ʱ�������ش�" << endl;
				stat = 4;
			}
			else if (pszRecv[0] == SEND)stat = 1;
			else if (pszRecv[0] == FIN) stat = 2;
			else stat = 100;
		}; break;
		case 1:
		{
			if (check_checksum(pszRecv, nRet))
			{
				cout << "У��ɹ�,����ACK���ݰ� pack=" << (int)pszRecv[1] << " txtlen=" << (int)pszRecv[2] << endl;
				if ((lastconfirm + 1) % 256 ^ pszRecv[1] == 0)
				{
					for (int i = 3; i < 3 + (int)pszRecv[2]; ++i)
						fout << pszRecv[i];
					lastconfirm++;
				}
				else
					cout << "��������ݰ�������" << endl;
			}
			else
			{
				cout << "У��ʧ��,�����ش� pack=" << (int)pszRecv[1] << " txtlen=" << (int)pszRecv[2] << endl;
				stat = 4;
			}
			stat = 3;
		}; break;
		case 2:
		{
			if (check_checksum(pszRecv, nRet))
			{
				cout << "FIN���ݰ�У��ɹ����������" << endl;
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

		}*/
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

int _tmain(int argc, _TCHAR* argv[])//_tmain,Ҫ�ӣ�include <tchar.h>������
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
	cout << Time;
	system("pause");
	return 0;
}
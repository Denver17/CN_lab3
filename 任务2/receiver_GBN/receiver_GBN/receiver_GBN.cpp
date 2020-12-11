// receiver_GBN.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include<WinSock2.h>

#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable : 4996)

using namespace std;


const int BUFFER_LENGTH = 1026;
const int SEQ_SIZE = 20;//接收端序列号个数，为 1~20  
BOOL cl_ack[SEQ_SIZE];//收到 ack 情况，对应 0~19 的 ack 
int cl_curSeq;//当前数据包的 seq 
int cl_curAck;//当前等待确认的 ack 
int cl_totalSeq;//收到的包的总数 
int cl_totalPacket;//需要发送的包总数 
const int SEND_WIND_SIZE = 10;

void timeoutHandler();

void ackHandler(char c);

bool seqIsAvailable();


//创建套接字
SOCKET createSocket();
//判断文件是否存在
BOOL is_dir_exist(char cdir[]);
//获取文件存储的根路径
char* get_file_path(char name[]);
//打开文件
void open_file(FILE*& p, char path[], SOCKET sSocket);

struct My_File
{
	long length;
	char buffer[BUFFER_LENGTH];
	int flag;
	//string success;
}my_file;


int main()
{
	//创建服务器套接字
	SOCKET s_socket = createSocket();

	//创建结构地址变量
	sockaddr_in server_addr;//服务器端地址
	sockaddr_in client_addr;//客户端地址
	int s_len = sizeof(server_addr);
	int c_len = sizeof(client_addr);

	//设置服务器的地址
	server_addr.sin_family = AF_INET;   //设置家族协议
	server_addr.sin_port = htons(8999); //设置端口号
	server_addr.sin_addr.S_un.S_addr = INADDR_ANY;

	//把地址绑定到服务器
	int ret = bind(s_socket, (SOCKADDR*)&server_addr, s_len);
	if (ret == SOCKET_ERROR)
	{
		cout << "bind failed " << endl;
		closesocket(s_socket);
		WSACleanup();
		cout << "20s后退出控制台！" << endl;
		Sleep(20000);
		exit(0);
	}


	char data[200];                        //接收短数据的缓冲区
	memset(data, 0, sizeof(data));         //初始化缓冲区
	char begin[] = "好的，准备接受图片。"; //开始标志消息
	char end[] = "接收图片完成。";         //结束标志消息
	int f_rcv;                              //接受状态
	int f_send;                             //发送状态
	FILE* p;                               //文件指针

	/* 发送的包较大，超过接受者缓存导致丢包：
	包超过mtu size数倍，几个大的udp包可能会超过接收者的缓冲，导致丢包。
	这种情况可以设置socket接收缓冲。 */
	//int nRecvBuf = 128 * 1024;//设置为128K
	//setsockopt(sSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&nRecvBuf, sizeof(int));

	//循环接收数据
	while (1)
	{
		cout << "=================================================Server===============================================" << endl;

		int waitCount = 0;
		int stage = 0;
		BOOL b;
		unsigned char u_code;//状态码    
		unsigned short seq;//包的序列号    
		unsigned short recvSeq;//接收窗口大小为 1，已确认的序列号    
		unsigned short waitSeq;//等待的序列号 

		for (int i = 0; i < SEQ_SIZE; ++i) {
			cl_ack[i] = TRUE;
		}

		//f_rcv = recvfrom(s_socket, data, sizeof(data), 0, (SOCKADDR*)&client_addr, &c_len);

	

		while (true)
		{
			
			//首先接收客户端的开始消息
			f_rcv = recvfrom(s_socket, my_file.buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&client_addr, &c_len);
			if (f_rcv == SOCKET_ERROR) {
				cout << "接受信息失败！" << endl;
				//cout << "2s后退出控制台！" << endl;
				//closesocket(s_socket);
				//WSACleanup();
				Sleep(2000);
				//exit(0);
			}
			
			if ((unsigned char)my_file.buffer[0] == 205) {
				printf("Ready for file transmission\n");
				my_file.buffer[0] = 200;
				my_file.buffer[1] = '\0';
				sendto(s_socket, my_file.buffer, 2, 0, (SOCKADDR*)&client_addr, c_len);
				stage = 1;
				recvSeq = 0;
				waitSeq = 1;
				cl_curSeq = 0;
				cl_curAck = 0;
				cl_totalSeq = 0;
				break;
			}
			////发送开始回馈消息给客户端
			//f_send = sendto(s_socket, begin, strlen(begin), 0, (SOCKADDR*)&client_addr, c_len);
			//if (f_send == SOCKET_ERROR) {
			//	cout << "发送信息失败！" << endl;
			//	cout << "2s后退出控制台！" << endl;
			//	closesocket(s_socket);
			//	WSACleanup();
			//	Sleep(2000);
			//	exit(0);
			//}
			//cout << "Server: " << begin << endl;
			//break;
			
		}


		//接收文件名字
		memset(data, 0, sizeof(data));
		f_rcv = recvfrom(s_socket, data, sizeof(data), 0, (SOCKADDR*)&client_addr, &c_len);
		if (f_rcv == SOCKET_ERROR) {
			cout << "接受信息失败！" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(s_socket);
			WSACleanup();
			Sleep(2000);
			exit(0);
		}

		//获取文件存放的根路径
		cout << "接收的文件名字: " << data << endl;

		//设置图像存放的目录，目录必须存在，输入的形式:
		//  D:\\test\\  或者D:/test/
		char* path = get_file_path(data);
		cout << "文件存放的目录：" << path << endl;
		memset(data, 0, sizeof(data));


		//cout << "Client: " << data << endl;

		

		//打开一个存放数据的文件
		open_file(p, path, s_socket);

		

		cout << "····接收中····" << endl;
		my_file.flag = 0;//接收完成的标志




		//循环接收客服端发送的数据包
		while (!my_file.flag) {
			memset(&my_file.buffer[2], 0, sizeof(my_file.buffer) - 2);
			//cout << "===================dsadhaiusd========================" << endl;
			f_rcv = recvfrom(s_socket, (char*)&my_file, sizeof(struct My_File), 0, (SOCKADDR*)&client_addr, &c_len);
			if (f_rcv == SOCKET_ERROR) {
				cout << "接受图片失败！" << endl;
				//cout << "2s后退出控制台！" << endl;
				//closesocket(s_socket);
				//WSACleanup();
				Sleep(20000000);
				//return -8;
			}

			seq = (unsigned short)my_file.buffer[0];

			if (waitSeq - seq == 0) {
				++waitSeq;
				if (waitSeq == 21) {
					waitSeq = 1;
				}
				//输出数据       
				//printf("%s\n",&buffer[1]);

				if (seqIsAvailable())
				{
					my_file.buffer[0] = seq;
					my_file.buffer[1] = cl_curSeq;
					cl_ack[cl_curSeq] = FALSE;
					//memcpy(&my_file.buffer[2], data + 1024 * cl_totalSeq, 1024);
					recvSeq = seq;
					++cl_curSeq;
					cl_curSeq %= SEQ_SIZE;
					++cl_totalSeq;
					Sleep(500);
				}	
				fwrite(&my_file.buffer[2], my_file.length, 1, p);
			}
			else {
				//如果当前一个包都没有收到，则等待 Seq 为 1 的数 据包，跳出循环，不返回 ACK（因为并没有上一个正确的 ACK）       
				if (recvSeq == 0) {
					continue;
				}
				if (seqIsAvailable())
				{
					my_file.buffer[0] = seq;
					my_file.buffer[1] = cl_curSeq;
					cl_ack[cl_curSeq] = FALSE;
					//memcpy(&my_file.buffer[2], data + 1024 * cl_totalSeq, 1024);
					recvSeq = seq;
					++cl_curSeq;
					cl_curSeq %= SEQ_SIZE;
					++cl_totalSeq;
				}
				//fwrite(&my_file.buffer[2], my_file.length - 2, 1, p);
			}


			if (f_rcv == 0) //客户端已经关闭连接
			{
				printf("Client has closed the connection\n");
				return 0;
			}

			f_send = sendto(s_socket, my_file.buffer, 2, 0, (SOCKADDR*)&client_addr, c_len);

			////接受一次包就确认一次
			//char success[] = "success";
			//f_send = sendto(s_socket, success, strlen(success), 0, (SOCKADDR*)&client_addr, c_len);
			//if (f_send == SOCKET_ERROR) {
			//	cout << "发送信息失败！" << endl;
			//	cout << "2s后退出控制台！" << endl;
			//	closesocket(s_socket);
			//	WSACleanup();
			//	Sleep(2000);
			//	return -10;
			//}
			//memset(success, 0, sizeof(success));
		}

		cout << "····接收中····" << endl;
		cout << "····接收完成····" << endl;
		//关闭文件
		fclose(p);
		p = NULL;
		cout << endl;


		//接收客户端的信息——我的文件发送完成。
		f_rcv = recvfrom(s_socket, data, sizeof(data), 0, (SOCKADDR*)&client_addr, &c_len);
		if (f_rcv == SOCKET_ERROR) {
			cout << "接受信息失败！" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(s_socket);
			WSACleanup();
			Sleep(2000);
			return -9;

		}
		cout << "Client: " << data << endl;
		memset(data, 0, sizeof(data));

		//发送接收图片完成。
		f_send = sendto(s_socket, end, strlen(end), 0, (SOCKADDR*)&client_addr, c_len);
		if (f_send == SOCKET_ERROR) {
			cout << "发送信息失败！" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(s_socket);
			WSACleanup();
			Sleep(2000);
			return -10;
		}
		cout << "Server: " << end << endl;


		//接收客户端是关闭连接还是继续发文件的标志
		f_rcv = recvfrom(s_socket, data, sizeof(data), 0, (SOCKADDR*)&client_addr, &c_len);
		if (f_rcv == SOCKET_ERROR) {
			cout << "接受信息失败！" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(s_socket);
			WSACleanup();
			Sleep(2000);
			return -11;
		}
		cout << "Client: " << data << endl;
		//若接收的信息等于拜拜，就关闭服务器连接
		if (!(strcmp(data, "byebye"))) {
			cout << "2m后关闭服务器连接！";
			Sleep(200000);
			break;
		}
		memset(data, 0, sizeof(data));
		cout << endl;
		cout << endl;

	}

	//关闭连接，清理
	closesocket(s_socket);
	WSACleanup();

	return 0;
}


//该函数用于创建socket
SOCKET createSocket() {
	WORD version = MAKEWORD(2, 2);
	WSADATA wsadata;
	if (WSAStartup(version, &wsadata))
	{
		cout << "WSAStartup failed " << endl;
		cout << "2s后控制台将会关闭！" << endl;
		Sleep(2000);
		exit(0);
	}
	//判断版本
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2)
	{
		cout << "wVersion not 2.2" << endl;
		cout << "2s后控制台将会关闭！" << endl;
		Sleep(2000);
		exit(0);
	}

	SOCKET sSocket;
	sSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (SOCKET_ERROR == sSocket)
	{
		cout << "socket failed" << endl;
		cout << "2s后控制台将会关闭！" << endl;
		Sleep(2000);
		exit(0);
	}
	else {
		return sSocket;
	}
}





// 该函数判断文件夹是否存在
BOOL is_dir_exist(char c_dir[])
{
	string dir(c_dir);
	size_t origsize = dir.length() + 1;
	const size_t newsize = 100;
	size_t convertedChars = 0;
	wchar_t* wcstring = (wchar_t*)malloc(sizeof(wchar_t) * (dir.length() - 1));
	mbstowcs_s(&convertedChars, wcstring, origsize, dir.c_str(), _TRUNCATE);
	DWORD dwAttrib = GetFileAttributes(wcstring);
	return (INVALID_FILE_ATTRIBUTES == dwAttrib) && (0 != (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
	//return 1;
}




//该函数获取文件存储的路径
char* get_file_path(char name[]) {
	char dir[50] = { 0 };
	//循环输入存放的目录，若电脑中不存在输入的目录，将要求重新输入
	while (true)
	{
		cout << "请输入文件存放路径: " << endl;
		cin >> dir;
		if (is_dir_exist(dir)) {
			break;
		}
		else {
			cout << "文件目录不存在！" << endl;
		}
	}
	int k = 0;
	int n_len = strlen(name);
	int d_len = strlen(dir);
	static char path[100];//存放指定的图像存放地址，此处变量必须是static型
	memset(path, 0, sizeof(path));//必须每一次进行初始化
	//将目录和文件名拼接在一起
	for (int i = 0; i < d_len; i++) {
		path[k++] = dir[i];
	}
	for (int j = 0; j < n_len; j++) {
		path[k++] = name[j];
	}
	//返回文件名
	return path;
}



//该函数用于打开文件所存放的位置
void open_file(FILE*& p, char path[], SOCKET s_socket) {
	// 以读 / 写方式打开或建立一个二进制文件，允许读和写。
	if (!(p = fopen(path, "wb+"))) {
		cout << "图片存放路径出错！" << endl;
		cout << "2s后退出控制台！" << endl;
		closesocket(s_socket);
		WSACleanup();
		Sleep(20000);
		exit(0);
	}
	cout << endl;

}





void timeoutHandler()
{
	printf("Timer out error.\n");
	int index;
	for (int i = 0; i < SEND_WIND_SIZE; ++i) {
		index = (i + cl_curAck) % SEQ_SIZE;
		cl_ack[index] = TRUE;
	}
	cl_totalSeq -= SEND_WIND_SIZE;
	cl_curSeq = cl_curAck;
}

void ackHandler(char c)
{
	if (c == 0)
	{
		printf("client:::server's first package do not have ack!\n");
		return;
	}
	unsigned char index = (unsigned char)c - 1; //序列号减一  
	printf("client:::Recv a ack of %d\n", index + 1);//序号发送方和接收方应该统一，发送的是+1过的，接收方Ack+1过的，在这里打印没必要还原
	if (cl_curAck <= index) {
		for (int i = cl_curAck; i <= index; ++i) {
			cl_ack[i] = TRUE;
		}
		cl_curAck = (index + 1) % SEQ_SIZE;
	}
	else {   //ack 超过了最大值，回到了 curAck 的左边   
		for (int i = cl_curAck; i < SEQ_SIZE; ++i) {
			cl_ack[i] = TRUE;
		}
		for (int i = 0; i <= index; ++i) {
			cl_ack[i] = TRUE;
		}
		cl_curAck = index + 1;
	}
}


bool seqIsAvailable()
{
	int step;
	step = cl_curSeq - cl_curAck;
	step = step >= 0 ? step : step + SEQ_SIZE;  //序列号是否在当前发送窗口之内  
	if (step >= SEND_WIND_SIZE) {
		return false;
	}
	if (cl_ack[cl_curSeq]) {
		return true;
	}
	return false;
}



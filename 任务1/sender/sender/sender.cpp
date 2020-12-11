// sender.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>

#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
using namespace std;
#pragma warning(disable : 4996)


//创建一个socket
SOCKET createSocket();
//打开文件
void open_file(char data[], FILE*& p);
//用于获取文件的存储路径
char* get_file_name(char path[]);
bool show(int f_send, SOCKET client, char discon[], sockaddr_in server_addr, int s_len);
//定义一个结构体，用于文件的传递
struct My_File {
	int length;         //统计每次真正发送的数据量
	char buffer[4096];  //用于发送数据缓冲区
	int flag;           //标记图片是否发送完成
}my_file;               //声明了一个结构体变量my_file；


int main()
{
	//创建客户端UDP套接字
	SOCKET client = createSocket();

	//创建地址结构体变量
	sockaddr_in server_addr;
	sockaddr_in client_addr;
	int s_len = sizeof(server_addr);
	int c_len = sizeof(client_addr);
	//设置服务器地址
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(8999);
	server_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	char data[100] = { 0 };                    //接受一些短字节的数据缓冲区
	char begin[] = "我要准备发图片了。";       //发送图片前的确认信息
	char end[] = "我的图片发送完成。";         //完成图片发送的通知信息
	char discon[] = "byebye";                  //关闭客户端的通知信息
	int f_send = 0;                             //发送函数的状态
	int f_recv = 0;                             //接收函数的状态
	FILE* p;                                   //创建一个文件指针
	static int count = 1;                      //用于统计发送文件的个数


	//循环向服务器发送图片
	while (1)
	{
		cout << "************************************第" << count << "次传输图片*******************************" << endl;

		//发送图片前先和服务器打个招呼，欲准备状态，
		//判断信息发送是否成功，若不成功，则服务器处于关闭状态

		f_send = sendto(client, begin, strlen(begin), 0, (SOCKADDR*)&server_addr, s_len);
		if (f_send == SOCKET_ERROR) {
			cout << "服务器处于关闭状态，请稍后重试！" << endl;
			cout << "2m后退出控制台！" << endl;
			closesocket(client);
			WSACleanup();
			Sleep(20000);
			return -4;
		}
		cout << "Client: " << begin << endl;


		//接受服务器的确认信息，判断信息接收是否成功，接收到信息后开始发送，否则关闭链接
		f_recv = recvfrom(client, data, sizeof(data), 0, (SOCKADDR*)&server_addr, &s_len);
		if (f_recv == SOCKET_ERROR) {
			cout << "接受确认信息失败！" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(client);
			WSACleanup();
			Sleep(2000);
			return -5;
		}
		cout << "Server: " << data << endl;
		memset(data, 0, sizeof(data));  //重新初始化data接收数据缓冲区


		//开始加载文件
		open_file(data, p);
		
		//获取发送图片的名字
		char* name = get_file_name(data);
		cout << "要发送的文件地址：" << name << endl;

		//发送文件名字
		f_send = sendto(client, name, strlen(name), 0, (SOCKADDR*)&server_addr, s_len);
		if (f_send == SOCKET_ERROR)
		{
			cout << "发送图片内容出错" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(client);
			WSACleanup();
			Sleep(2000);
			return -7;
		}
		memset(data, 0, sizeof(data));

		//计算文件的总长度
		fseek(p, 0, SEEK_END);  //指针移动到图片的最后一个字节；
		int length = ftell(p);  //获取图片总长度
		fseek(p, 0, SEEK_SET);  //指针还原到开始位置

		//分包发送图片，图片长度大于0，循环发送，否则发送完毕，停止发送
		cout << endl;
		cout << "····发送准备中····" << endl;

		while (length > 0)
		{
			memset(my_file.buffer, 0, sizeof(my_file.buffer));     //初始化接受缓冲区
			
			fread(my_file.buffer, sizeof(my_file.buffer), 1, p);   //读取文件到缓冲区
			int len = sizeof(my_file.buffer);                      //获取读取的长度


			/*若读取的长度大于当前文件剩余总长度，将结构体的文件长度赋值为图片剩余长度，
			并标记文件读取结束；否则文件长度为读取缓冲区长度，文件标记状态为未完成 */
			if (length >= len) {
				my_file.flag = 0;
				my_file.length = len;
			}
			else {
				my_file.length = length;
				my_file.flag = 1;
			}

			//发送图片的一部分，发送成功，则图片总长度减去当前发送的图片长度
			f_send = sendto(client, (char*)&my_file, sizeof(struct My_File), 0, (SOCKADDR*)&server_addr, s_len);
			if (f_send == SOCKET_ERROR) {
				cout << "发送图片出错" << endl;
				cout << "2s后退出控制台！" << endl;
				closesocket(client);
				WSACleanup();
				Sleep(2000);
				return -8;
			}
			else {
				length -= len;
			}

			//接受服务器的确认信息
			f_recv = recvfrom(client, data, sizeof(data), 0, (SOCKADDR*)&server_addr, &s_len);
			if (f_recv == SOCKET_ERROR) {
				cout << "接受确认信息失败！" << endl;
				cout << "10s后退出控制台！" << endl;
				closesocket(client);
				WSACleanup();
				Sleep(10000);
				return -10;
			}

			//若收到确认消息为success则继续下一轮的传输；否则退出控制台
			if (strcmp(data, "success") != 0) {
				cout << "图片部分发送失败！" << endl;
				cout << "10s后退出控制台！" << endl;
				closesocket(client);
				WSACleanup();
				Sleep(10000);
				return -10;
			}
			//清空data，进入下一轮
			memset(data, 0, sizeof(data));

		}


		cout << "····发送中····" << endl;
		cout << "····发送完成····" << endl;
		cout << endl;

		//发送消息给服务器，告诉他已经发送完毕
		f_send = sendto(client, end, strlen(end), 0, (SOCKADDR*)&server_addr, s_len);
		if (f_send == SOCKET_ERROR)
		{
			cout << "发送消息出错！" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(client);
			WSACleanup();
			Sleep(2000);
			return -9;
		}
		cout << "Client: " << end << endl;


		//接受服务器的接收完成的确认信息
		f_recv = recvfrom(client, data, sizeof(data), 0, (SOCKADDR*)&server_addr, &s_len);
		if (f_recv == SOCKET_ERROR) {
			cout << "接受确认信息失败！" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(client);
			WSACleanup();
			Sleep(2000);
			return -10;
		}
		cout << "Server: " << data << endl;
		memset(data, 0, sizeof(data));


		//是否继续发送图片
		bool f = show(f_send, client, discon, server_addr, s_len);
		if (f) {
			count++;
		}
		else
		{
			break;
		}
		fclose(p);
		p = NULL;
		cout << endl;
		cout << endl;


	}

	//关闭
	closesocket(client);
	WSACleanup();


	return 0;
}


SOCKET createSocket() {
	//声明调用不同的Winsock版本。
	WORD version = MAKEWORD(2, 2);
	//一种数据结构。这个结构被用来存储被WSAStartup函数调用后返回的Windows Sockets数据。
	WSADATA wsadata;
	/*WSAStartup必须是应用程序或DLL调用的第一个Windows Sockets函数。
	它允许应用程序或DLL指明Windows Sockets API的版本号及获得特定Windows Sockets实现的细节。
	应用程序或DLL只能在一次成功的WSAStartup()调用之后才能调用进一步的Windows Sockets API函数。
	*/
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
	//创建客户端UDP套接字
	SOCKET client;
	client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (SOCKET_ERROR == client)
	{
		cout << "socket failed" << endl;
		cout << "2s后控制台将会关闭！" << endl;
		Sleep(2000);
		exit(0);
	}
	else {
		return client;
	}
}


//打开文件函数，特别注意是传递引用，不然后续操作会出错
void open_file(char data[], FILE*& p) {
	while (1) {
		cout << "请输入要发送文件的路径: " << endl;
		cin >> data;  //输入图片的绝对路径
		// 以读 / 写方式打开一个二进制文件，只允许读 / 写数据。若文件无法打开，则路径有问题，关闭连接
		cout << "输入的文件名：" << data << endl;
		if (!(p = fopen(data, "rb+"))) {
			memset(data, 0, sizeof(data));
			cout << "文件路径出错,请重新尝试！" << endl;
		}
		else {
			break;
		}
	}
}


//获取文件的名称，去掉父目录
char* get_file_name(char path[]) {
	static char name[20];
	memset(name, 0, sizeof(name));
	int len = strlen(path);
	int count = 0;
	//输入的目录形如：D:\\test\\1.jpg 或者D:/test/1.jpg     名称为1.jpg
	//从最后一个字母开始查找，查找第一个出现的"/"或者"\"
	for (int i = len - 1; i > 0; i--) {
		if (path[i] != '\\'&& path[i] != '/') {
			count++;
		}
		else {
			break;
		}
	}
	int j = 0;
	int pos = len - count;
	for (int i = pos; i < len; i++) {
		name[j++] = path[i];
	}
	cout << "name：" << name << endl;
	return name;
}


bool show(int iSend, SOCKET client, char discon[], sockaddr_in sever_addr, int s_len) {
	int j;
	cout << endl;
	cout << "请选择是否继续发送图片：（1或者2）" << endl;
	cout << "   1: YES   2: NO " << endl;
	cout << "我的选择: ";
	cin >> j;
	if (j == 2) {
		iSend = sendto(client, discon, strlen(discon), 0, (SOCKADDR*)&sever_addr, s_len);
		if (iSend == SOCKET_ERROR)
		{
			cout << "发送消息出错！" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(client);
			WSACleanup();
			Sleep(2000);
			exit(0);
		}
		cout << "Clent: " << discon << endl;
		cout << "30s后关闭服务器连接！";
		Sleep(30000);
		return  FALSE;
	}
	else {
		iSend = sendto(client, "请接受下一张图片。", strlen("请接受下一张图片。"), 0, (SOCKADDR*)&sever_addr, s_len);
		if (iSend == SOCKET_ERROR)
		{
			cout << "发送消息出错！" << endl;
			cout << "2m后退出控制台！" << endl;
			closesocket(client);
			WSACleanup();
			Sleep(20000);
			exit(0);
		}
		cout << "Client: 请接受下一张图片。" << endl;
		return TRUE;
	}
}








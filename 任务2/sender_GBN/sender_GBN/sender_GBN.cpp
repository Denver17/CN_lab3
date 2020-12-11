// sender_GBN.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"

#include <iostream>
#include <time.h>
#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
using namespace std;
#pragma warning(disable : 4996)

const int BUFFER_LENGTH = 1026;  //缓冲区大小，（以太网中 UDP 的数据 帧中包长度应小于 1480 字节） 

const int SEND_WIND_SIZE = 10;
//发送窗口大小为 10，GBN 中应满足 W + 1 <= N（W 为发送窗口大小，N 为序列号个数）
//本例取序列号 0...19 共 20 个 
//如果将窗口大小设为 1，则为停-等协议  
const int SEQ_SIZE = 20;
//序列号的个数，从 0~19 共计 20 个
//由于发送数据第一个字节如果值为 0，则数据会发送 失败
//因此接收端序列号为 1~20，与发送端一一对应  
BOOL ack[SEQ_SIZE];//收到 ack 情况，对应 0~19 的 ack 
int cur_seq;//当前数据包的 seq 
int cur_ack;//当前等待确认的 ack 
int total_seq;//收到的包的总数 
int total_packet;//需要发送的包总数 


void getCurTime(char *ptime);

bool seqIsAvailable();

void timeoutHandler();

void ackHandler(char c);



//创建一个socket
SOCKET createSocket();
//打开文件
void open_file(char data[], FILE*& p);
//用于获取文件的存储路径
char* get_file_name(char path[]);
bool show(int f_send, SOCKET client, char discon[], sockaddr_in server_addr, int s_len);
//定义一个结构体，用于文件的传递
struct My_File {
	int length;					 //统计每次真正发送的数据量
	char buffer[BUFFER_LENGTH];  //用于发送数据缓冲区
	int flag;					//标记文件是否发送完成
}my_file;						//声明了一个结构体变量my_file；


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
	char begin[] = "我要准备发文件了。";       //发送图片前的确认信息
	char end[] = "我的文件发送完成。";         //完成图片发送的通知信息
	char discon[] = "byebye";                  //关闭客户端的通知信息
	int f_send = 0;                             //发送函数的状态
	int f_recv = 0;                             //接收函数的状态
	FILE* p;                                   //创建一个文件指针
	static int count = 1;                      //用于统计发送文件的个数


	//循环向服务器发送图片
	while (1)
	{
		cout << "************************************第" << count << "次传输文件*******************************" << endl;

		//发送图片前先和服务器打个招呼，欲准备状态，
		//判断信息发送是否成功，若不成功，则服务器处于关闭状态

		unsigned short seq;//包的序列号    
		unsigned short recv_seq;//接收窗口大小为 1，已确认的序列号    
		unsigned short wait_seq;//等待的序列号 

		for (int i = 0; i < SEQ_SIZE; ++i) {
			ack[i] = TRUE;
		}

		int recvSize;
		int waitCount = 0;
		//加入了一个握手阶段    
		//首先服务器向客户端发送一个 205 大小的状态码（我自己定义的） 表示服务器准备好了，可以发送数据    
		//客户端收到 205 之后回复一个 200 大小的状态码，表示客户端准 备好了，可以接收数据了    
		//服务器收到 200 状态码之后，就开始使用 GBN 发送数据了    
		int stage = 0;
		bool run_flag = true;




		while (run_flag)//三次握手
		{
			if (stage == 0)
			{

				//发送 205 阶段   
				my_file.buffer[0] = 205;
				sendto(client, my_file.buffer, strlen(my_file.buffer) + 1, 0, (SOCKADDR*)&server_addr, s_len);
				Sleep(100);
				stage = 1;
			}


			/*if (f_send == SOCKET_ERROR) {
				cout << "服务器处于关闭状态，请稍后重试！" << endl;
				cout << "2m后退出控制台！" << endl;
				closesocket(client);
				WSACleanup();
				Sleep(20000);
				return -4;
			}*/
			//cout << "Client: " << begin << endl;


			if (stage == 1)
			{

				//等待接收 200 阶段，没有收到则计数器+1，超时则 放弃此次“连接”，等待从第一步开始 
				f_recv = recvfrom(client, my_file.buffer, BUFFER_LENGTH, 0, ((SOCKADDR*)&server_addr), &s_len);
				if (f_recv < 0) {
					++waitCount;
					if (waitCount > 20) {
						run_flag = false;
						printf("Timeout error\n");
						break;
					}
					Sleep(500);
					continue;
				}
				else {
					if ((unsigned char)my_file.buffer[0] == 200) {
						printf("Begin a file transfer\n");
						//printf("File size is %dB, each packet is 1024B and packet total num is %d\n", sizeof(data), total_packet);
						cur_seq = 0;
						cur_ack = 0;
						total_seq = 0;
						waitCount = 0;
						//stage = 2;
						recv_seq = 0;
						wait_seq = 1;
						break;
					}
				}
			}
		}



		////接受服务器的确认信息，判断信息接收是否成功，接收到信息后开始发送，否则关闭链接
		//f_recv = recvfrom(client, data, sizeof(data), 0, (SOCKADDR*)&server_addr, &s_len);
		//if (f_recv == SOCKET_ERROR) {
		//	cout << "接受确认信息失败！" << endl;
		//	cout << "2s后退出控制台！" << endl;
		//	closesocket(client);
		//	WSACleanup();
		//	Sleep(2000);
		//	return -5;
		//}
		//cout << "Server: " << data << endl;
		//memset(data, 0, sizeof(data));  //重新初始化data接收数据缓冲区

		//开始加载文件
		open_file(data, p);

		//获取发送图片的名字
		char* name = get_file_name(data);
		cout << "要发送的文件地址：" << name << endl;

		//发送文件名字
		f_send = sendto(client, name, strlen(name), 0, (SOCKADDR*)&server_addr, s_len);
		if (f_send == SOCKET_ERROR)
		{
			cout << "发送文件内容出错" << endl;
			cout << "2s后退出控制台！" << endl;
			closesocket(client);
			WSACleanup();
			Sleep(2000);
			return -1;
		}
		memset(data, 0, sizeof(data));

		//计算文件的总长度
		fseek(p, 0, SEEK_END);  //指针移动到图片的最后一个字节；
		int length = ftell(p);  //获取文件总长度
		fseek(p, 0, SEEK_SET);  //指针还原到开始位置


		while (run_flag)
		{

			//数据传输阶段       
			//分包发送图片，图片长度大于0，循环发送，否则发送完毕，停止发送
			cout << endl;
			cout << "····发送准备中····" << endl;

			while (length > 0)
			{
				memset(&my_file.buffer[2], 0, sizeof(my_file.buffer) - 2);     //初始化接受缓冲区

				fread(&my_file.buffer[2], sizeof(my_file.buffer) - 2, 1, p);   //读取文件到缓冲区
				int len = sizeof(my_file.buffer) - 2;                      //获取读取的长度

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

				//cout << "发送" << endl;

				if (seqIsAvailable()) //判断序列号是否有效
				{
					//cout << "发送2" << endl;
					//发送给客户端的序列号从 1 开始 
					my_file.buffer[0] = cur_seq + 1;//seq
					ack[cur_seq] = FALSE;
					//数据发送的过程中应该判断是否传输完成
					//为简化过程此处并未实现
					my_file.buffer[1] = recv_seq;
					//memcpy(&(my_file.buffer[2]), data + 1024 * total_seq, 1024);
					printf("server:::send a packet with a seq of %d,ack of %d  \n", cur_seq + 1, my_file.buffer[1]);
					//sendto(client, my_file.buffer, BUFFER_LENGTH, 0, (SOCKADDR*)&server_addr, sizeof(SOCKADDR));
					++cur_seq;
					cur_seq %= SEQ_SIZE;
					++total_seq;
					//Sleep(500);
				}       //等待 Ack，若没有收到，则返回值为-1，计数器+1



			
				//发送图片的一部分，发送成功，则图片总长度减去当前发送的图片长度
				f_send = sendto(client, (char*)&my_file, sizeof(struct My_File), 0, (SOCKADDR*)&server_addr, s_len);
				if (f_send == SOCKET_ERROR) {
					cout << "发送图片出错" << endl;
					//cout << "2s后退出控制台！" << endl;
					//closesocket(client);
					//WSACleanup();
					Sleep(2000);
					//return -8;
				}
				else {
					length -= len;
				}

				//接受服务器的确认信息
				f_recv = recvfrom(client, my_file.buffer, 2, 0, (SOCKADDR*)&server_addr, &s_len);
				//cout << "recv" << endl;
				if (f_recv == SOCKET_ERROR) {
					waitCount++;        //20 次等待 ack 则超时重传        
					if (waitCount > 20) {
						timeoutHandler();
						waitCount = 0;
					}
					cout << "接受确认信息失败！" << endl;
					//cout << "10s后退出控制台！" << endl;
					//closesocket(client);
					//WSACleanup();
					Sleep(10000);
					//return -10;
				}
				else
				{
					//收到 客户端ack和客户端发送的数据；
					seq = (unsigned short)my_file.buffer[1];
					if (wait_seq - seq == 0)//收到的序列号正好是所等待的序列号：接收ACK，数据
					{
						recv_seq = seq;
						++wait_seq;
						if (wait_seq == 20) {
							wait_seq = 1;
						}
						printf("server:::Recv a client packet of %d\n", seq);//打印来自客户端数据的序号
						ackHandler(my_file.buffer[0]);
					}
					else
					{//没有接收到数据
						printf("server:::Recv a client packet of %d\n", seq);//打印来自客户端数据的序号
						ackHandler(my_file.buffer[0]);
					}
					//Sleep(500);
					//break;

				}
				////若收到确认消息为success则继续下一轮的传输；否则退出控制台
				//if (strcmp(data, "success") != 0) {
				//	cout << "图片部分发送失败！" << endl;
				//	cout << "10s后退出控制台！" << endl;
				//	closesocket(client);
				//	WSACleanup();
				//	Sleep(10000);
				//	return -10;
				//}
				////清空data，进入下一轮
				//memset(data, 0, sizeof(data));

			}
			break;
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



//获取系统时间
time_t t = time(nullptr);

void getCurTime(char *ptime)
{
	char buffer[128];
	memset(buffer, 0, sizeof(buffer));
	time_t c_time;
	struct tm *p;
	time(&c_time);
	p = (tm *)localtime_s((tm *)&c_time, &t);
	sprintf_s(buffer, "%d/%d/%d %d:%d:%d", p->tm_year + 1900, p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min,
		p->tm_sec);  strcpy_s(ptime, sizeof(buffer), buffer);
}

//判断当前序列号是否可用
bool seqIsAvailable()
{
	int step;
	step = cur_seq - cur_ack;
	step = step >= 0 ? step : step + SEQ_SIZE;  //序列号是否在当前发送窗口之内  
	if (step >= SEND_WIND_SIZE) {
		return false;
	}
	if (ack[cur_seq]) {
		return true;
	}
	return false;
}


//超时重传函数
void timeoutHandler()
{
	printf("Timer out error.\n");
	int index;
	for (int i = 0; i < SEND_WIND_SIZE; ++i) {
		index = (i + cur_ack) % SEQ_SIZE;
		ack[index] = TRUE;
	}
	total_seq -= SEND_WIND_SIZE;
	cur_seq = cur_ack;
}


//累积确认函数
void ackHandler(char c)
{
	unsigned char index = (unsigned char)c - 1; //序列号减一  
	printf("server:::Recv a ack of %d\n", index + 1);//序号发送方和接收方应该统一，发送的是+1过的，接收方Ack+1过的，在这里打印没必要还原
	if (cur_ack <= index) {
		for (int i = cur_ack; i <= index; ++i) {
			ack[i] = TRUE;
		}
		cur_ack = (index + 1) % SEQ_SIZE;
	}
	else {   //ack 超过了最大值，回到了 curAck 的左边   
		for (int i = cur_ack; i < SEQ_SIZE; ++i) {
			ack[i] = TRUE;
		}
		for (int i = 0; i <= index; ++i) {
			ack[i] = TRUE;
		}
		cur_ack = index + 1;
	}
}



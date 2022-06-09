//#include "stdafx.h"
#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507 //发送数据报文的最大长度
#define HTTP_PORT 80 //http 服务器端口

#define INVILID_WEBSITE "http://today.hit.edu.cn/"   //屏蔽网址
#define FISH_WEB_SRC "http://www.4399.com/"   //钓鱼源网址
#define fish_web_url "http://jwts.hit.edu.cn/"   //钓鱼目的网址
#define fish_web_host "jwts.hit.edu.cn"           //钓鱼目的地址的主机名



//Http 重要头部数据
struct HttpHeader {
	char method[4]; // POST 或者GET，注意有些为CONNECT，本实验暂不考虑
	char url[1024]; // 请求的url
	char host[1024]; // 目标主机
	char cookie[1024 * 10]; //cookie
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));
	}
};

//代理相关参数
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 8080; // 代理端口号


struct ProxyParam {
	// 代理参数，分别定义客户端和服务端套接字
	SOCKET clientSocket;
	SOCKET serverSocket;
};

BOOL InitSocket();
BOOL ParseHttpHead(char* buffer, HttpHeader* httpHeader);
BOOL ConnectToServer(SOCKET* serverSocket, char* host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
void makeCache(char* buffer, char* url);
void getCache(char* buffer, char* filename);
void makeNewHTTP(char* buffer, char* value);
void getDate(char* buffer, char* field, char* tempDate);
void makeFilename(char* url, char* filename);

//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: 初始化套接字
//************************************
BOOL InitSocket() {

	//加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本2.2
	wVersionRequested = MAKEWORD(2, 2); // 将两个byte型合成一个word型
	//加载dll 文件Scoket 库  向操作系统说明，我们需要哪个库文件，让该库文件与当前应用程序绑定
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//找不到winsock.dll
		printf("加载winsock 失败，错误代码为: %d\n", WSAGetLastError());
		return FALSE;
	}

	// 获得低位字节和高位字节，判断版本是否匹配
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("不能找到正确的winsock 版本\n");
		WSACleanup();
		return FALSE;
	}

	//AF_INET,PF_INET IPV4 Internet协议
	//SOCK_STREAM Tcp连接，提供序列化可靠双向连接
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0); // IPV4地址族，流套接字，0
	if (ProxyServer == INVALID_SOCKET) {
		printf("创建套接字失败，错误代码为：%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET; // IPv4

	//将整型变量从主机字节顺序转变成网络字节顺序
	ProxyServerAddr.sin_port = htons(ProxyPort);



	//ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	// 将一个点分十进制的IP转换成一个长整数型数（u_long类型）
	ProxyServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("绑定套接字失败\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("监听端口%d 失败", ProxyPort);
		return FALSE;
	}
	return TRUE;
}


//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: 线程执行函数
// Parameter: LPVOID lpParameter
// //返回无符号整数，__stdcall说明函数从右向左通过堆栈传递
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {

	BOOL haveCache = false;
	BOOL needCache = true;

	char Buffer[MAXSIZE];
	char* CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE); // 用0来填充一块区域

	char fileBuffer[MAXSIZE];

	char filename[100];

	HttpHeader* httpHeader = new HttpHeader();


	// sockaddr_in ? 
	SOCKADDR_IN clientAddr;
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;

	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);

	CacheBuffer = new char[recvSize + 1];
	ZeroMemory(CacheBuffer, recvSize + 1);
	memcpy(CacheBuffer, Buffer, recvSize);

	if (recvSize <= 0) {
		goto error;
	}


	//解析http首部
	if (!ParseHttpHead(CacheBuffer, httpHeader))
	{
		goto error;
	}
	delete[] CacheBuffer;

	FILE* in;

	makeFilename(httpHeader->url, filename);

	char* field;
	field = (char*)"Date";

	char date_str[30];
	ZeroMemory(date_str, 30);

	// 是否已经有缓存
	if ((in = fopen(filename, "rb")) != NULL)
	{
		fread(fileBuffer, sizeof(char), MAXSIZE, in);
		fclose(in);

		getDate(fileBuffer, field, date_str);

		printf("date_str:%s\n", date_str);
		makeNewHTTP(Buffer, date_str);
		haveCache = true;
	}


	//屏蔽网站功能：
	if (strcmp(httpHeader->url, INVILID_WEBSITE) == 0)
	{
		printf("\n***********该网站已被屏蔽***********\n");
		goto error;
	}

	//网站引导：将访问网址转到其他网站
	if (strcmp(httpHeader->url, FISH_WEB_SRC) == 0)
	{
		printf("\n*******目标网址已被引导*******\n");
		memcpy(httpHeader->host, fish_web_host, strlen(fish_web_host) + 1);
		memcpy(httpHeader->url, fish_web_url, strlen(fish_web_url));
	}

	// 是否连接到需要访问的网址
	if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, httpHeader->host)) {
		goto error;
	}

	printf("代理连接主机 %s 成功\n", httpHeader->host);



	//将客户端发送的HTTP 数据报文直接转发给目标服务器
	ret = send(((ProxyParam*)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);

	//等待目标服务器返回数据
	recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		goto error;
	}
	// 是否有缓存，一般来说false
	if (haveCache)
	{
		getCache(Buffer, filename);
	}
	else {
		makeCache(Buffer, httpHeader->url);  //缓存报文
	}

	//将目标服务器返回的数据直接转发给客户端
	// 第一个发送到socket， 第二个要发的东西，第三个实际发送的东西，返回实际发送的字节
	ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);


	//错误处理
error:
	printf("关闭套接字\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	_endthreadex(0);
	return 0;
}


//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: 解析TCP 报文中的HTTP 头部
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
BOOL ParseHttpHead(char* buffer, HttpHeader* httpHeader) {
	char* p;
	char* ptr;
	const char* delim = "\r\n";

	p = strtok_s(buffer, delim, &ptr);//提取第一行

	printf("%s\n", p);

	if (p[0] == 'G') {//GET 方式
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P') {//POST 方式
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	//else if (p[0] == 'C') {
	//	// connect
	//	return false;
	//}

	printf("url是%s\n", httpHeader->url);
	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		switch (p[0]) {
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://Cookie
			if (strlen(p) > 8) {
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")) {
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
	return true;
}


//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: 根据主机创建目标服务器套接字，并连接
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET* serverSocket, char* host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT* hostent = gethostbyname(host);
	if (!hostent) {
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET) {
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr))
		== SOCKET_ERROR) {
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}


void makeCache(char* buffer, char* url) {
	char* p, * ptr, num[10], tempBuffer[MAXSIZE + 1];

	const char* delim = "\r\n";

	ZeroMemory(num, 10);
	ZeroMemory(tempBuffer, MAXSIZE + 1);
	memcpy(tempBuffer, buffer, strlen(buffer));

	p = strtok(tempBuffer, delim);//提取第一行
	memcpy(num, &p[9], 3);

	if (strcmp(num, "200") == 0) {  //状态码是200时缓存
		// 200指成功访问，404就是没成功

		// 构建文件
		char filename[100];  
		ZeroMemory(filename, 100);
		makeFilename(url, filename);
		printf("filename : %s\n", filename);

		FILE* out;
		out = fopen(filename, "w");
		fwrite(buffer, sizeof(char), strlen(buffer), out);
		fclose(out);
		printf("\n************************************\n\n");	             
		printf("\n网页已经被缓存\n");
	}
}

void getCache(char* buffer, char* filename) {
	char* p, * ptr, num[10], tempBuffer[MAXSIZE + 1];
	const char* delim = "\r\n";
	ZeroMemory(num, 10);
	ZeroMemory(tempBuffer, MAXSIZE + 1);

	memcpy(tempBuffer, buffer, strlen(buffer));

	p = strtok(tempBuffer, delim);//提取第一行
	memcpy(num, &p[9], 3);
	if (strcmp(num, "304") == 0) {  //主机返回的报文中的状态码为304时返回已缓存的内容
		printf("\n*************************************\n\n");
		printf("从本机获得缓存\n");
		ZeroMemory(buffer, strlen(buffer));
		FILE* in = NULL;
		if ((in = fopen(filename, "r")) != NULL) {
			fread(buffer, sizeof(char), MAXSIZE, in);
			fclose(in);
		}
	}
}

void makeNewHTTP(char* buffer, char* value) {
	const char* field = "Host";
	const char* newfield = "If-Modified-Since: ";
	//const char *delim = "\r\n";
	char temp[MAXSIZE];
	ZeroMemory(temp, MAXSIZE);

	char* pos = strstr(buffer, field);
	int i = 0;
	for (i = 0; i < strlen(pos); i++) {
		temp[i] = pos[i];
	}
	*pos = '\0';
	while (*newfield != '\0') {  //插入If-Modified-Since字段
		*pos++ = *newfield++;
	}
	while (*value != '\0') {
		*pos++ = *value++;
	}
	*pos++ = '\r';
	*pos++ = '\n';
	for (i = 0; i < strlen(temp); i++) {
		*pos++ = temp[i];
	}
}

void getDate(char* buffer, char* field, char* tempDate) {
	char* p, * ptr, temp[5];
	ZeroMemory(temp, 5);
	//*field = "If-Modified-Since";

	const char* delim = "\r\n";
	p = strtok(buffer, delim); // 按行读取
	//printf("%s\n", p);
	int len = strlen(field) + 2;
	while (p) {
		if (strstr(p, field) != NULL) {
			// 如果p中包含field字串，将&p[6]copy给tempdate
			memcpy(tempDate, &p[len], strlen(p) - len);
			// printf("tempDate: %s\n", tempDate);
		}
		p = strtok(NULL, delim);
	}
}

void makeFilename(char* url, char* filename) {
	int count = 0;
	while (*url != '\0') {
		if ((*url >= 'a' && *url <= 'z') || (*url >= 'A' && *url <= 'Z') || (*url >= '0' && *url <= '9')) {
			*filename++ = *url;
			count++;
		}
		if (count >= 95)
			break;
		url++;
	}
	strcat(filename, ".txt");
}


//由于新的连接都使用新线程进行处理，对线程的频繁的创建和销毁特别浪费资源
//可以使用线程池技术提高服务器效率
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};


int main(int argc, char* argv[])
{
	printf("代理服务器正在启动\n");
	printf("初始化...\n");

	// 将本地地址和套接字绑定，并监听套接字的链接请求
	// 套接字设置为监听模式
	if (!InitSocket()) {
		printf("socket 初始化失败\n");
		return -1;
	}
	printf("代理服务器正在运行，监听端口 %d\n", ProxyPort);

	SOCKET acceptSocket = INVALID_SOCKET; // 初始化接收套接字
	ProxyParam* lpProxyParam; //初始化代理参数，内包含客户端和服务端套接字
	HANDLE hThread;

	DWORD dwThreadID;

	//代理服务器不断监听
	while (true) {
		acceptSocket = accept(ProxyServer, NULL, NULL);

		lpProxyParam = new ProxyParam;

		if (lpProxyParam == NULL) {
			continue;
		}

		lpProxyParam->clientSocket = acceptSocket;

		// 创建子线程，执行一对一的代理过程
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);

		CloseHandle(hThread);
		Sleep(200);
	}
	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}


















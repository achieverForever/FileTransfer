#include <WinSock2.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <math.h>
#include <time.h>
#include <windows.h>
using namespace std;
#pragma comment(lib, "ws2_32.lib")

#define MAX_FILE_NAME 100	//最大文件名长度
#define DATA_PACK_SIZE 80*1000	// 每个DataPack结构大小为 80 KB
#define SOCKKET_BUFFER_SIZE 128*1000	  // socket缓冲区为128KB，用于暂存DataPack结构体
#define FILE_BUFFER_SIZE  100*1000  //文件缓冲区为100KB,用于暂存DataPack.content
#define CONTENT_SIZE DATA_PACK_SIZE-MAX_FILE_NAME-5*sizeof(int)  // DataPack结构体中的content的容量为79880 bytes.


// Define a structure to hold the content of a file
typedef struct FilePack{
	char fName[MAX_FILE_NAME];	// File's name
	unsigned long fLen;	// File's length
	unsigned long packNum;	// DataPack 编号
	int packLen;	// DataPack's length
	unsigned long packCount;	// 总共需多少个DataPack
	int contenLen;	// DataPack中content实际存储的字节数
	char content[CONTENT_SIZE];	// 用于存放文件字节流
}DataPack, *pDataPack;

void WinsockInitial(){
	WSADATA wsaData;
	WORD wVersionRequested;
	int err;
	wVersionRequested=MAKEWORD(2,2);
	err=WSAStartup(wVersionRequested, &wsaData);
	if(err!=0){
		cout<<"Error at WSAStartup()."<<endl;
		exit(0);
	}

	if( LOBYTE(wsaData.wVersion)!=2 || HIBYTE(wsaData.wVersion)!=2 ){
		cout<<"Error at version of Winsock. "<<endl;
		WSACleanup();
		exit(0);
	}
}

void SockBind(SOCKET sock, int port, sockaddr_in &addrsock){
	addrsock.sin_family=AF_INET;
	addrsock.sin_port=htons(port);
	addrsock.sin_addr.S_un.S_addr=htonl(INADDR_ANY);
	if( bind(sock, (sockaddr*)&addrsock, sizeof(addrsock)) == SOCKET_ERROR ){
		cout<<"Error at bind(). Error: "<<GetLastError()<<endl;
		closesocket(sock);
		WSACleanup();
		exit(0);
	}
}

void SockListen(SOCKET sock, int bak){
	int err=listen(sock, bak);
	if(err==SOCKET_ERROR){
		cout<<"Error at listen()."<<WSAGetLastError()<<endl;
		closesocket(sock);
		WSACleanup();
		exit(0);
	}
}

int SockSend(DataPack &dataPack, SOCKET &sock, char *sockBuf){
	int bytesLeft=0, bytesSend=0;
	int idx=0;

	bytesLeft=sizeof(dataPack);

	// 将DataPack结构体打入sockBuf缓冲区
	memcpy(sockBuf, &dataPack, sizeof(dataPack));

	while(bytesLeft>0){
		memset(sockBuf, 0, sizeof(sockBuf));

		bytesSend=send(sock, &sockBuf[idx], bytesLeft, 0);
		cout<<"一次send()之后, bytesSend: "<<bytesSend<<endl;
		if(bytesSend==SOCKET_ERROR){
			cout<<"Error at send()."<<endl;
			cout<<"Error # "<<WSAGetLastError()<<" happened."<<endl; 
			return 1;
		}
		bytesLeft-=bytesSend;
		idx+=bytesSend;
	}
	cout<<"DataPack 结构体发送成功"<<endl;
	return 0;
}

/*
unsigned long GetFileLen(FILE *fp){
	// 获取文件大小
	if(fp==NULL){
		cout<<"Invalid argument. Error at GetFileLen()."<<endl;
		exit(0);
	}
	fseek(fp, 0, SEEK_END);
	unsigned long tempFileLen=ftell(fp);
	cout<<ftell(fp)<<endl;
	fseek(fp, 0, SEEK_SET);
	return tempFileLen;
}
*/

//新版GetFileLen函数 
unsigned long GetFileLen(char *fileName){
	HANDLE hFile=CreateFile(fileName,GENERIC_READ,
		FILE_SHARE_READ|FILE_SHARE_WRITE,NULL, 
		OPEN_EXISTING ,FILE_ATTRIBUTE_NORMAL,NULL);
	if(hFile==INVALID_HANDLE_VALUE)
	{
		cout<<"error by opening file error:"<<GetLastError()<<endl<<fileName<<endl;
		return 0;
	}      

	LARGE_INTEGER lisize={0}; 

	if(!GetFileSizeEx(hFile,&lisize)){
		cout<<"GetFileSieEx() error:"<<GetLastError()<<endl;
	}
	cout<<"Use GetFileSizeEx()\tfileName:"<<fileName<<"\tFileSize:"<<lisize.QuadPart<<" bytes"<<endl;
	return lisize.QuadPart;
	CloseHandle(hFile);
}


int FileExists(FILE *fp, char *fileName) 
{ 
	
	if((fp=fopen(fileName, "r"))==NULL){ 
		cout<<"File not exist."<<endl;
		return 1;
	} 
	else{ 
		fclose(fp);
		return 0;
	}
}


int main(){
	
	int err;
	sockaddr_in addrServ;
	int port=8000;	//默认服务器为8000端口

	int Content_Size=79880;
	char fileName[MAX_FILE_NAME];
	unsigned long fileLen=0;

	//检测发送文件是否存在，若不存在，提示重新输入合法的文件名
	FILE *tmpfp=NULL;
	do {
		cout<<"传点啥？  ";
		cin>>fileName;

	} while (FileExists(tmpfp, fileName)!=0);



	// Initialize Winsock
	WinsockInitial();

	// Create a socket to listen
	SOCKET sockListen=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockListen==INVALID_SOCKET){
		cout<<"Error at socket()."<<endl;
		WSACleanup();
		return 1;
	}

	// Bind the socket with server's ip and port
	SockBind(sockListen, port, addrServ);
	

	// Listen for incoming connection requests
	cout<<"Waiting for incoming connection requests..."<<endl;
	SockListen(sockListen, 5);
	
	// Accept the connection request.
	sockaddr_in addrClient;
	int len=sizeof(addrClient);
	SOCKET sockConn=accept(sockListen, (sockaddr*)&addrClient, &len);
	if(sockConn!=INVALID_SOCKET){
		cout<<"Connected to client successfully."<<endl;
		cout<<"Client IP: "<<inet_ntoa(addrClient.sin_addr)<<endl;;
	}else{
		cout<<"Error at accept(). "<<WSAGetLastError()<<endl;
	}


	// Set the buffer size of socket
	char sockBuf[SOCKKET_BUFFER_SIZE];
	int nBuf=SOCKKET_BUFFER_SIZE;
	int nBufLen=sizeof(nBuf);
	err=setsockopt(sockConn, SOL_SOCKET, SO_SNDBUF, (char*)&nBuf, nBufLen);
	if(err!=0){
		cout<<"Error at setsockopt(). Failed to set buffer size for socket."<<endl;
		return 1;
	}

	//检查缓冲区是否设置成功
	err = getsockopt(sockConn, SOL_SOCKET, SO_SNDBUF, (char*)&nBuf, &nBufLen);
	if( SOCKKET_BUFFER_SIZE != nBuf){
		cout<<"Error at setsockopt(). 为socket创建缓冲区失败。"<<endl;
		closesocket(sockListen);
		closesocket(sockConn);
		WSACleanup();
		return 1;
	}

	//------------------------------------------------------------------------//


	DataPack dataPackSend;   // Declare DataPack structure

	int bytesRead;
	int bytesLeft;
	int bytesSend;
	int bytesLast;


	int i=0;
	int bSend=0;

	unsigned long packCount=0; // Counts how many DataPack needed

	FILE *frp;	// Used to read

	// Open the file in read+binary mode
	frp=fopen(fileName, "rb");
	if(frp==NULL){
		cout<<"Error at fopen_s()."<<endl;
		return 1;
	}

	cout<<"Setting File Buffer Size."<<endl;
	char fileBuf[FILE_BUFFER_SIZE];

	// Set the buffer size of File
	if(setvbuf(frp, fileBuf, _IONBF, FILE_BUFFER_SIZE)!=0){
		cout<<"Error at setvbuf().Failed to set buffer size for file."<<endl;
		closesocket(sockListen);
		closesocket(sockConn);
		WSACleanup();
		exit(0);
	}

	// Get file's length
	cout<<"Getting file length..."<<endl;
	fileLen=GetFileLen(fileName);
	cout<<"要传输的文件大小为："<<fileLen<<" bytes."<<endl;


	// Calculate how many DataPacks needed
	cout<<"Calculating packCount..."<<endl;
	cout<<"Now, fileLen is: "<<fileLen<<"       CONTENT_SIZE is "<<Content_Size<<endl;
	packCount=ceil( (double)fileLen / Content_Size);
	cout<<"共需要"<<packCount<<" 个DataPack"<<endl;
	cout<<endl;


	//计时
	clock_t start, finish; 
	double   duration; 
	start = clock(); 


	//开始发送DataPack结构体
	for(i=0; i<packCount; i++){

		memset(&dataPackSend, 0, sizeof(dataPackSend));
		memset(fileBuf, 0, sizeof(fileBuf));
		memset(sockBuf, 0, sizeof(sockBuf));

		//填充DataPackSend
		strcpy(dataPackSend.fName, fileName);

		dataPackSend.fLen=fileLen;
		dataPackSend.packCount=packCount;
		dataPackSend.packLen=DATA_PACK_SIZE;


		if( packCount==1 ){	   // fLen < CONTENT_SIZE, 一个DataPack就够了,将文件内容全部写入fileBuf

			bytesRead=fread(fileBuf, 1, dataPackSend.fLen, frp);
			if(bytesRead<dataPackSend.fLen){
				cout<<"Error at fread().";
				return 1;
			}
			cout<<"读取唯一一个dataPack后， 文件位置指针： "<<ftell(frp)<<endl;
			cout<<"packCount为1时，文件内容全部读入fileBuf成功"<<endl;

			dataPackSend.contenLen=dataPackSend.fLen;

			memcpy(dataPackSend.content, fileBuf, dataPackSend.fLen);

			dataPackSend.packNum=0;	//唯一也是第一个DataPack
			cout<<"DataPack 结构体填充完毕，准备发送"<<endl;

			// 发送结构体dataPackSend到Client端

			memcpy(sockBuf, (char*)&dataPackSend, sizeof(dataPackSend));
			bSend=send(sockConn, sockBuf, sizeof(dataPackSend), 0);
			if(bSend < sizeof(dataPackSend) ){
				cout<<"Error at send(). 发送第一个（唯一）DataPack时错误."<<endl;
			}
			
		}else if(packCount>1 && i<(packCount-1)){	// 中间包(含第一个普通包)	
			
			bytesRead=fread(fileBuf, 1, Content_Size, frp);

			if(bytesRead<Content_Size){
				cout<<"Error at fread().";
				return 1;
			}
			
			dataPackSend.contenLen=CONTENT_SIZE;

			memcpy(dataPackSend.content, fileBuf, CONTENT_SIZE);

			dataPackSend.packNum=i;	
			
			//发送DataPackSend
			memcpy(sockBuf, (char*)&dataPackSend, sizeof(dataPackSend));
/*OLD
			bSend=send(sockConn, sockBuf, sizeof(dataPackSend), 0);
			if(bSend < sizeof(dataPackSend) ){
				cout<<"Error at send(). 发送第 "<<dataPackSend.packNum<<" 个DataPack时错误."<<endl;
			}else{
					cout<<"成功发送 "<<dataPackSend.packNum<<" 号 DataPack."<<endl;
			}
*/		
			//发送数据包	new
			bytesLeft=sizeof(dataPackSend);
			int idx=0;
			while(bytesLeft>0){
				bSend=send(sockConn, &sockBuf[idx], bytesLeft, 0);
				if(bSend==SOCKET_ERROR){
					cout<<"Error at send(). 发送第 "<<dataPackSend.packNum<<" 个DataPack时出错. Error #"<<WSAGetLastError()<<endl;
					return 1;
				}else{
					cout<<"成功发送 "<<dataPackSend.packNum<<" 号 DataPack. "<<endl;
				}
				idx=bSend;
				bytesLeft-=bSend;

			}

		}else if( i==(packCount-1) ){	// 最后一个包

			bytesLast=dataPackSend.fLen-(packCount-1)*Content_Size;

			bytesRead=fread(fileBuf, 1, bytesLast, frp);
			if(bytesRead<bytesLast){
				cout<<"Error at fread().";
				cout<<"最后一个包出错"<<endl;
				return 1;
			}


			dataPackSend.contenLen=bytesLast;

			memcpy(dataPackSend.content, fileBuf, dataPackSend.contenLen);

			dataPackSend.packNum=i;
	
			//将dataPackSend发送给Client端
			memcpy(sockBuf, (char*)&dataPackSend, sizeof(dataPackSend));
			bSend=send(sockConn, sockBuf, sizeof(dataPackSend), 0);
			if(bSend < sizeof(dataPackSend) ){
				cout<<"Error at send(). 发送第 "<<dataPackSend.packNum<<" 个DataPack时错误."<<endl;
			}else{
				cout<<"成功发送第 "<<dataPackSend.packNum<<" DataPack (最后一个). "<<endl;
			}

		}else{
			cout<<"For循环i值有问题."<<endl;
		}

	}

	//计时
	finish=clock();
	duration = (double)(finish - start) / CLOCKS_PER_SEC; 


	fclose(frp);

	closesocket(sockListen);
	closesocket(sockConn);

	WSACleanup();

	double fileLenMB=fileLen/(1024.0*1024.0);
	cout<<"传输文件大小："<<fileLen<<" bytes."<<endl;
	cout<<"文件大小： "<<fileLenMB<<" MB"<<endl;
	double speed=fileLenMB/duration;
	cout<<"传输文件 "<<fileName<<" 共用时 "<<duration<<" sec.   平均速度 "<<speed<<" MB/s."<<endl;


	return 0;
}
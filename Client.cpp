#include <WinSock2.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
using namespace std;
#pragma comment(lib, "ws2_32.lib")


#define MAX_FILE_NAME 100	//最大文件名长度
#define DATA_PACK_SIZE 80*1000	// 每个DataPack结构大小为 80 KB
#define SOCKKET_BUFFER_SIZE 128*1000	  // socket缓冲区为128KB，用于暂存DataPack结构体
#define FILE_BUFFER_SIZE  100*1000  //文件缓冲区为100KB,用于暂存DataPack.content
#define CONTENT_SIZE DATA_PACK_SIZE-MAX_FILE_NAME-5*sizeof(int)  // DataPack结构体中的content的容量为79880 bytes.

// 定义数据包结构
typedef struct FilePack{
	char fName[MAX_FILE_NAME];	// 文件名
	unsigned long fLen;			// 文件长度
	unsigned long packNum;		// 数据包编号
	int packLen;				// 数据包长度
	unsigned long packCount;	// 数据包总数
	int contenLen;				// 数据包实际携带数据的字节数
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

int SockRecv(SOCKET sock, char *sockBuf, DataPack &dataPack){
	int bytesLeft=0, bytesRecv=0;
	int idx=0;

	bytesLeft=sizeof(DataPack);

	while(bytesLeft>0){
		bytesRecv=recv(sock, &sockBuf[idx], bytesLeft, 0);
		if(bytesRecv==SOCKET_ERROR){
			cout<<"Error at recv()."<<endl;
			cout<<"Error # "<<WSAGetLastError()<<" happened."<<endl; 
			return 1;
		}
		bytesLeft-=bytesRecv;
		idx+=bytesRecv;
	}
	memcpy(&dataPack, sockBuf, sizeof(dataPack));
	return 0;
}

int FileExists(FILE *fp, char *fileName) 
{ 

	if((fp=fopen(fileName, "r"))==NULL){ 
		cout<<fileName<<" not exist."<<endl;
		return 1;
	} 
	else{ 
		cout<<fileName<<" already exist."<<endl;
		fclose(fp);
		return 0;
	}
}

int main(){
	int err;
	sockaddr_in addrServ;
	char servIP[20];
	cout<<"请输入 Server IP: ";
	cin>>servIP;
	int port=8000;

	unsigned long packCount=0;

	static unsigned long fileLen=0;

	int bytesRecv=0, bytesLeft=0, bytesWrite=0, bRecv=0;
	static unsigned long bytesRecvCount=0;	//统计已接收多少字节数据,用于与fileLen比较，检测文件是否完全被接收成功。

	DataPack dataPackRecv;	

	// 初始化winsock
	WinsockInitial();

	// 创建一个Socket
	SOCKET sockClient=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockClient==INVALID_SOCKET){
		cout<<"Error at socket()."<<endl;
		WSACleanup();
		return 1;
	}

	// 设置Socket缓冲区大小
	char sockBuf[SOCKKET_BUFFER_SIZE];
	int nBuf=SOCKKET_BUFFER_SIZE;
	int nBufLen=sizeof(nBuf);
	err=setsockopt(sockClient, SOL_SOCKET, SO_RCVBUF, (char*)&nBuf, nBufLen);
	if(err!=0){
		cout<<"Error at setsockopt(). Failed to set buffer size for socket."<<endl;
		return 1;
	}

	//检查缓冲区是否设置成功
	err = getsockopt(sockClient, SOL_SOCKET, SO_RCVBUF, (char*)&nBuf, &nBufLen);
	if( SOCKKET_BUFFER_SIZE != nBuf){
		cout<<"Error at getsockopt(). 为socket创建缓冲区失败。"<<endl;
		closesocket(sockClient);
		WSACleanup();
		return 1;
	}

	// 与服务器建立连接
	addrServ.sin_family=AF_INET;
	addrServ.sin_port=htons(port);
	addrServ.sin_addr.S_un.S_addr=inet_addr(servIP);
	err=connect(sockClient, (sockaddr*)&addrServ, sizeof(sockaddr));
	if(err==SOCKET_ERROR){
		cout<<"Error at connect()."<<GetLastError()<<"The Server 还没打开呢！"<<endl;
		closesocket(sockClient);
		WSACleanup();
		return 1;
	}else{
		cout<<"Connected to the FTP Server successfully."<<endl;
	}

	//计时
	clock_t start, finish; 
	double  duration; 
	start = clock(); 


	//接收第0个包，从第0个DataPack中获取文件名，文件大小等信息，以提示用户是否接受

	bRecv=recv(sockClient, sockBuf, sizeof(DataPack), 0);
	cout<<"正在接受第0个DataPack."<<endl;

	memcpy(&dataPackRecv, sockBuf, sizeof(dataPackRecv));

	//获取数据包数量
	packCount=dataPackRecv.packCount;
	fileLen=dataPackRecv.fLen;

	char fileName[100];

	memcpy(fileName, dataPackRecv.fName, sizeof(dataPackRecv.fName));

	bytesRecvCount=bytesRecvCount+dataPackRecv.contenLen;
	cout<<"bytesRecvCount: "<<bytesRecvCount<<endl;
	cout<<"从第0个包中获取到的数据, packCount: "<<packCount<<"    fileLen: "<<fileLen<<endl;

	cout<<"Ftp Server "<<servIP<<" 向您发送文件 "<<fileName<<" ,接收按'y',否则按'n'. "<<endl;
	char select;
	cin>>select;
	if(select=='n'){
		cout<<"您选择了拒绝接收这个文件."<<endl;
		return 1;
	}

	// 在本地创建远端文件的拷贝
	FILE *fwp=NULL;

	// 检查是否存在同名文件
	if(FileExists(fwp, dataPackRecv.fName)==0){
		cout<<"Do you want to overload it ? (y for Yes and n for No)"<<endl;
		cin>>select;
		if(select=='n'){
			cout<<"A prefix 'New_' will be automatically added to new file's name."<<endl;
			char tmpfileName[MAX_FILE_NAME];
			strcpy(tmpfileName, "New_");
			strncat(tmpfileName, fileName, 96);
			memcpy(fileName, tmpfileName, 100);
		}
	}

	fwp=fopen(fileName, "wb");
	if(fwp==NULL){
		cout<<"Error at fopen(). Failed to create a local file. "<<endl;
	}
	if(err!=0){
		cout<<"Error at creat fopen_s(). Failed to create a local file to write into."<<endl;
		return 1;
	}

	char fileBuf[FILE_BUFFER_SIZE];

	if(setvbuf(fwp, fileBuf, _IONBF, FILE_BUFFER_SIZE)!=0){
		cout<<"Error at setvbuf().Failed to set buffer size for file."<<endl;
		memset(fileBuf, 0, sizeof(fileBuf));
		closesocket(sockClient);
		WSACleanup();
		return 1;
	}

	//将第一个包中的content写入文件
	memcpy(fileBuf, dataPackRecv.content, dataPackRecv.contenLen);
	bytesWrite=fwrite(fileBuf, 1, dataPackRecv.contenLen, fwp);

	if(bytesWrite<dataPackRecv.contenLen){
		cout<<"Error at fwrite(). Failed to write the content of dataPackRecv to local file."<<endl;
		return 1;
	}else{
		cout<<"成功将第0个DataPack中的 "<<bytesWrite<<" bytes 数据写入文件。"<<endl;
	}
	
	int i;
	//开始循环从packCount-1个数据包中取出数据后写到本地同名文件中
	if(packCount>1){
		for(i=1; i<packCount; i++){

			memset(sockBuf, 0, sizeof(sockBuf));
			memset(&dataPackRecv, 0, sizeof(dataPackRecv));
			memset(fileBuf, 0, sizeof(fileBuf));

			//接收剩余的数据包	new
			bytesLeft=sizeof(dataPackRecv);
			int idx=0;
			while(bytesLeft>0){
				bRecv=recv(sockClient, &sockBuf[idx], bytesLeft, 0);
				if(bRecv==SOCKET_ERROR){
					cout<<"Error at recv(). "<<WSAGetLastError()<<endl;
					return 1;
				}
				idx=bRecv;
				bytesLeft-=bRecv;

			}

		
			memcpy(&dataPackRecv, sockBuf, sizeof(DataPack));

			bytesRecvCount+=dataPackRecv.contenLen;


			//写入文件
			memcpy(fileBuf, dataPackRecv.content, dataPackRecv.contenLen);

			bytesWrite=fwrite(fileBuf, 1, dataPackRecv.contenLen, fwp);
			cout<<"正在接收 "<<dataPackRecv.packNum<<" 个DataPack.      bytesRecvCount: "<<bytesRecvCount<<endl;
			if(bytesWrite<dataPackRecv.contenLen){
				cout<<"Error at fwrite(). Failed to write the content of dataPackRecv to local file."<<endl;
			}
		}
	}

	cout<<"fileLen: "<<fileLen<<"    "<<"bytesRecvCount: "<<bytesRecvCount<<endl;
	
	if( bytesRecvCount==fileLen ){
		cout<<"已成功接受 "<<bytesRecvCount<<" bytes 数据。"<<endl;
		cout<<"所有DataPack都已成功接收完成！"<<endl;
	}else{
		cout<<"传输文件数据丢失，请重试！"<<endl;
		cout<<"文件将被删除"<<endl;
		remove(fileName);
	}
	fclose(fwp);
	closesocket(sockClient);
	WSACleanup();

	//计时
	finish=clock();
	duration = (double)(finish - start) / CLOCKS_PER_SEC; 
	double fileLenMB=fileLen/(1024.0*1024.0);
	double speed=fileLenMB/duration;
	cout<<"fileLen: "<<fileLen<<" bytes     "<<"packCount: "<<packCount<<endl;
	cout<<"文件大小："<<fileLen<<" bytes."<<endl;
	cout<<"文件大小 "<<fileLenMB<<" MB"<<endl;
	cout<<"传输文件 "<<fileName<<" 共用时 "<<duration<<" sec.   平均速度 "<<speed<<" MB/s."<<endl;
	system("pause");
	return 0;
}

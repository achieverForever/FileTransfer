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

#define MAX_FILE_NAME 100	//����ļ�������
#define DATA_PACK_SIZE 80*1000	// ÿ��DataPack�ṹ��СΪ 80 KB
#define SOCKKET_BUFFER_SIZE 128*1000	  // socket������Ϊ128KB�������ݴ�DataPack�ṹ��
#define FILE_BUFFER_SIZE  100*1000  //�ļ�������Ϊ100KB,�����ݴ�DataPack.content
#define CONTENT_SIZE DATA_PACK_SIZE-MAX_FILE_NAME-5*sizeof(int)  // DataPack�ṹ���е�content������Ϊ79880 bytes.


// Define a structure to hold the content of a file
typedef struct FilePack{
	char fName[MAX_FILE_NAME];	// File's name
	unsigned long fLen;	// File's length
	unsigned long packNum;	// DataPack ���
	int packLen;	// DataPack's length
	unsigned long packCount;	// �ܹ�����ٸ�DataPack
	int contenLen;	// DataPack��contentʵ�ʴ洢���ֽ���
	char content[CONTENT_SIZE];	// ���ڴ���ļ��ֽ���
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

	// ��DataPack�ṹ�����sockBuf������
	memcpy(sockBuf, &dataPack, sizeof(dataPack));

	while(bytesLeft>0){
		memset(sockBuf, 0, sizeof(sockBuf));

		bytesSend=send(sock, &sockBuf[idx], bytesLeft, 0);
		cout<<"һ��send()֮��, bytesSend: "<<bytesSend<<endl;
		if(bytesSend==SOCKET_ERROR){
			cout<<"Error at send()."<<endl;
			cout<<"Error # "<<WSAGetLastError()<<" happened."<<endl; 
			return 1;
		}
		bytesLeft-=bytesSend;
		idx+=bytesSend;
	}
	cout<<"DataPack �ṹ�巢�ͳɹ�"<<endl;
	return 0;
}

/*
unsigned long GetFileLen(FILE *fp){
	// ��ȡ�ļ���С
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

//�°�GetFileLen���� 
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
	int port=8000;	//Ĭ�Ϸ�����Ϊ8000�˿�

	int Content_Size=79880;
	char fileName[MAX_FILE_NAME];
	unsigned long fileLen=0;

	//��ⷢ���ļ��Ƿ���ڣ��������ڣ���ʾ��������Ϸ����ļ���
	FILE *tmpfp=NULL;
	do {
		cout<<"����ɶ��  ";
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

	//��黺�����Ƿ����óɹ�
	err = getsockopt(sockConn, SOL_SOCKET, SO_SNDBUF, (char*)&nBuf, &nBufLen);
	if( SOCKKET_BUFFER_SIZE != nBuf){
		cout<<"Error at setsockopt(). Ϊsocket����������ʧ�ܡ�"<<endl;
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
	cout<<"Ҫ������ļ���СΪ��"<<fileLen<<" bytes."<<endl;


	// Calculate how many DataPacks needed
	cout<<"Calculating packCount..."<<endl;
	cout<<"Now, fileLen is: "<<fileLen<<"       CONTENT_SIZE is "<<Content_Size<<endl;
	packCount=ceil( (double)fileLen / Content_Size);
	cout<<"����Ҫ"<<packCount<<" ��DataPack"<<endl;
	cout<<endl;


	//��ʱ
	clock_t start, finish; 
	double   duration; 
	start = clock(); 


	//��ʼ����DataPack�ṹ��
	for(i=0; i<packCount; i++){

		memset(&dataPackSend, 0, sizeof(dataPackSend));
		memset(fileBuf, 0, sizeof(fileBuf));
		memset(sockBuf, 0, sizeof(sockBuf));

		//���DataPackSend
		strcpy(dataPackSend.fName, fileName);

		dataPackSend.fLen=fileLen;
		dataPackSend.packCount=packCount;
		dataPackSend.packLen=DATA_PACK_SIZE;


		if( packCount==1 ){	   // fLen < CONTENT_SIZE, һ��DataPack�͹���,���ļ�����ȫ��д��fileBuf

			bytesRead=fread(fileBuf, 1, dataPackSend.fLen, frp);
			if(bytesRead<dataPackSend.fLen){
				cout<<"Error at fread().";
				return 1;
			}
			cout<<"��ȡΨһһ��dataPack�� �ļ�λ��ָ�룺 "<<ftell(frp)<<endl;
			cout<<"packCountΪ1ʱ���ļ�����ȫ������fileBuf�ɹ�"<<endl;

			dataPackSend.contenLen=dataPackSend.fLen;

			memcpy(dataPackSend.content, fileBuf, dataPackSend.fLen);

			dataPackSend.packNum=0;	//ΨһҲ�ǵ�һ��DataPack
			cout<<"DataPack �ṹ�������ϣ�׼������"<<endl;

			// ���ͽṹ��dataPackSend��Client��

			memcpy(sockBuf, (char*)&dataPackSend, sizeof(dataPackSend));
			bSend=send(sockConn, sockBuf, sizeof(dataPackSend), 0);
			if(bSend < sizeof(dataPackSend) ){
				cout<<"Error at send(). ���͵�һ����Ψһ��DataPackʱ����."<<endl;
			}
			
		}else if(packCount>1 && i<(packCount-1)){	// �м��(����һ����ͨ��)	
			
			bytesRead=fread(fileBuf, 1, Content_Size, frp);

			if(bytesRead<Content_Size){
				cout<<"Error at fread().";
				return 1;
			}
			
			dataPackSend.contenLen=CONTENT_SIZE;

			memcpy(dataPackSend.content, fileBuf, CONTENT_SIZE);

			dataPackSend.packNum=i;	
			
			//����DataPackSend
			memcpy(sockBuf, (char*)&dataPackSend, sizeof(dataPackSend));
/*OLD
			bSend=send(sockConn, sockBuf, sizeof(dataPackSend), 0);
			if(bSend < sizeof(dataPackSend) ){
				cout<<"Error at send(). ���͵� "<<dataPackSend.packNum<<" ��DataPackʱ����."<<endl;
			}else{
					cout<<"�ɹ����� "<<dataPackSend.packNum<<" �� DataPack."<<endl;
			}
*/		
			//�������ݰ�	new
			bytesLeft=sizeof(dataPackSend);
			int idx=0;
			while(bytesLeft>0){
				bSend=send(sockConn, &sockBuf[idx], bytesLeft, 0);
				if(bSend==SOCKET_ERROR){
					cout<<"Error at send(). ���͵� "<<dataPackSend.packNum<<" ��DataPackʱ����. Error #"<<WSAGetLastError()<<endl;
					return 1;
				}else{
					cout<<"�ɹ����� "<<dataPackSend.packNum<<" �� DataPack. "<<endl;
				}
				idx=bSend;
				bytesLeft-=bSend;

			}

		}else if( i==(packCount-1) ){	// ���һ����

			bytesLast=dataPackSend.fLen-(packCount-1)*Content_Size;

			bytesRead=fread(fileBuf, 1, bytesLast, frp);
			if(bytesRead<bytesLast){
				cout<<"Error at fread().";
				cout<<"���һ��������"<<endl;
				return 1;
			}


			dataPackSend.contenLen=bytesLast;

			memcpy(dataPackSend.content, fileBuf, dataPackSend.contenLen);

			dataPackSend.packNum=i;
	
			//��dataPackSend���͸�Client��
			memcpy(sockBuf, (char*)&dataPackSend, sizeof(dataPackSend));
			bSend=send(sockConn, sockBuf, sizeof(dataPackSend), 0);
			if(bSend < sizeof(dataPackSend) ){
				cout<<"Error at send(). ���͵� "<<dataPackSend.packNum<<" ��DataPackʱ����."<<endl;
			}else{
				cout<<"�ɹ����͵� "<<dataPackSend.packNum<<" DataPack (���һ��). "<<endl;
			}

		}else{
			cout<<"Forѭ��iֵ������."<<endl;
		}

	}

	//��ʱ
	finish=clock();
	duration = (double)(finish - start) / CLOCKS_PER_SEC; 


	fclose(frp);

	closesocket(sockListen);
	closesocket(sockConn);

	WSACleanup();

	double fileLenMB=fileLen/(1024.0*1024.0);
	cout<<"�����ļ���С��"<<fileLen<<" bytes."<<endl;
	cout<<"�ļ���С�� "<<fileLenMB<<" MB"<<endl;
	double speed=fileLenMB/duration;
	cout<<"�����ļ� "<<fileName<<" ����ʱ "<<duration<<" sec.   ƽ���ٶ� "<<speed<<" MB/s."<<endl;


	return 0;
}
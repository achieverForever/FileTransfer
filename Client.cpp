#include <WinSock2.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
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
	cout<<"������ Server IP: ";
	cin>>servIP;
	int port=8000;

	unsigned long packCount=0;

	static unsigned long fileLen=0;

	int bytesRecv=0, bytesLeft=0, bytesWrite=0, bRecv=0;
	static unsigned long bytesRecvCount=0;	//ͳ���ѽ��ն����ֽ�����,������fileLen�Ƚϣ�����ļ��Ƿ���ȫ�����ճɹ���

	DataPack dataPackRecv;	// Used to store a DataPack structure

	// Initialize Winsock
	WinsockInitial();

	// Create a socket
	SOCKET sockClient=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sockClient==INVALID_SOCKET){
		cout<<"Error at socket()."<<endl;
		WSACleanup();
		return 1;
	}

	// Set the buffer size of socket
	char sockBuf[SOCKKET_BUFFER_SIZE];
	int nBuf=SOCKKET_BUFFER_SIZE;
	int nBufLen=sizeof(nBuf);
	err=setsockopt(sockClient, SOL_SOCKET, SO_RCVBUF, (char*)&nBuf, nBufLen);
	if(err!=0){
		cout<<"Error at setsockopt(). Failed to set buffer size for socket."<<endl;
		return 1;
	}

	//��黺�����Ƿ����óɹ�
	err = getsockopt(sockClient, SOL_SOCKET, SO_RCVBUF, (char*)&nBuf, &nBufLen);
	if( SOCKKET_BUFFER_SIZE != nBuf){
		cout<<"Error at getsockopt(). Ϊsocket����������ʧ�ܡ�"<<endl;
		closesocket(sockClient);
		WSACleanup();
		return 1;
	}

	// Connect to the Server
	addrServ.sin_family=AF_INET;
	addrServ.sin_port=htons(port);
	addrServ.sin_addr.S_un.S_addr=inet_addr(servIP);
	err=connect(sockClient, (sockaddr*)&addrServ, sizeof(sockaddr));
	if(err==SOCKET_ERROR){
		cout<<"Error at connect()."<<GetLastError()<<"The Server ��û���أ�"<<endl;
		closesocket(sockClient);
		WSACleanup();
		return 1;
	}else{
		cout<<"Connected to the FTP Server successfully."<<endl;
	}

	//��ʱ
	clock_t start, finish; 
	double  duration; 
	start = clock(); 


	//���յ�0�������ӵ�0��DataPack�л�ȡ�ļ������ļ���С����Ϣ������ʾ�û��Ƿ����

	bRecv=recv(sockClient, sockBuf, sizeof(DataPack), 0);
	cout<<"���ڽ��ܵ�0��DataPack."<<endl;

	memcpy(&dataPackRecv, sockBuf, sizeof(dataPackRecv));

	//��ȡ���ݰ�����
	packCount=dataPackRecv.packCount;
	fileLen=dataPackRecv.fLen;

	char fileName[100];

	memcpy(fileName, dataPackRecv.fName, sizeof(dataPackRecv.fName));

	bytesRecvCount=bytesRecvCount+dataPackRecv.contenLen;
	cout<<"bytesRecvCount: "<<bytesRecvCount<<endl;
	cout<<"�ӵ�0�����л�ȡ��������, packCount: "<<packCount<<"    fileLen: "<<fileLen<<endl;

	cout<<"Ftp Server "<<servIP<<" ���������ļ� "<<fileName<<" ,���հ�'y',����'n'. "<<endl;
	char select;
	cin>>select;
	if(select=='n'){
		cout<<"��ѡ���˾ܾ���������ļ�."<<endl;
		return 1;
	}

	// Create a local file to write into
	FILE *fwp=NULL;

	// Check if the file with the same file name already exist, if exists, ask client if he/her want to overload the existed one
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

	// Set the buffer size of File
	char fileBuf[FILE_BUFFER_SIZE];

	if(setvbuf(fwp, fileBuf, _IONBF, FILE_BUFFER_SIZE)!=0){
		cout<<"Error at setvbuf().Failed to set buffer size for file."<<endl;
		memset(fileBuf, 0, sizeof(fileBuf));
		closesocket(sockClient);
		WSACleanup();
		return 1;
	}

	//����һ�����е�contentд���ļ�
	memcpy(fileBuf, dataPackRecv.content, dataPackRecv.contenLen);

	cout<<"fwrite()ǰ��λ��ָ�룺 "<<ftell(fwp)<<endl;
	bytesWrite=fwrite(fileBuf, 1, dataPackRecv.contenLen, fwp);
	cout<<"fwrite()��λ��ָ�룺 "<<ftell(fwp)<<endl;


	if(bytesWrite<dataPackRecv.contenLen){
		cout<<"Error at fwrite(). Failed to write the content of dataPackRecv to local file."<<endl;
		return 1;
	}else{
		cout<<"�ɹ�����0��DataPack�е� "<<bytesWrite<<" bytes ����д���ļ���"<<endl;
	}
	
	int i;
	//��ʼѭ����packCount-1�����ݰ���ȡ�����ݺ�д������ͬ���ļ���
	if(packCount>1){
		for(i=1; i<packCount; i++){

		// ����ʣ������ݰ�
			memset(sockBuf, 0, sizeof(sockBuf));
			memset(&dataPackRecv, 0, sizeof(dataPackRecv));
			memset(fileBuf, 0, sizeof(fileBuf));
/*	Old	
			bRecv=recv(sockClient, sockBuf, sizeof(dataPackRecv), 0);
			if(bRecv<sizeof(dataPackRecv)){
				cout<<"Error at FIRST recv(). Failed to receive a complete DataPack structure."<<endl;
				int _bRecv=0;
				_bRecv=recv(sockClient, &sockBuf[bRecv], sizeof(dataPackRecv)-bRecv, 0);
				if( _bRecv < (sizeof(dataPackRecv)-bRecv) ){
					cout<<"Error at SECOND recv(). Failed to receive a complete DataPack structure. "<<endl;
					return 1;
				}

			}	*/

			//����ʣ������ݰ�	new
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

		//	cout<<"���ڽ��յ� "<<dataPackRecv.packNum<<" ��DataPack."<<endl;
		//	cout<<"���ڽ��ܵ� "<<dataPackRecv.packNum<<" ��DataPack."<<endl;
		//	cout<<"Ӧ�����ֽڣ� "<<sizeof(dataPackRecv)<<" bytes."<<endl;
		//	cout<<"�ѽ����ֽڣ�"<<bRecv<<" bytes."<<endl;

			bytesRecvCount+=dataPackRecv.contenLen;


			//д���ļ�
			memcpy(fileBuf, dataPackRecv.content, dataPackRecv.contenLen);

		//	cout<<"fwrite()ǰ�� "<<ftell(fwp)<<endl;
			bytesWrite=fwrite(fileBuf, 1, dataPackRecv.contenLen, fwp);
		//	cout<<"fwrite()�� "<<ftell(fwp)<<endl;
			cout<<"���ڽ��� "<<dataPackRecv.packNum<<" ��DataPack.      bytesRecvCount: "<<bytesRecvCount<<endl;
			if(bytesWrite<dataPackRecv.contenLen){
				cout<<"Error at fwrite(). Failed to write the content of dataPackRecv to local file."<<endl;
			}
		}
	}

	cout<<"fileLen: "<<fileLen<<"    "<<"bytesRecvCount: "<<bytesRecvCount<<endl;
	
	if( bytesRecvCount==fileLen ){
		cout<<"�ѳɹ����� "<<bytesRecvCount<<" bytes ���ݡ�"<<endl;
		cout<<"����DataPack���ѳɹ�������ɣ�"<<endl;
	}else{
		cout<<"�����ļ����ݶ�ʧ�������ԣ�"<<endl;
		cout<<"�ļ�����ɾ��"<<endl;
		remove(fileName);
	}
	fclose(fwp);
	closesocket(sockClient);
	WSACleanup();

	//��ʱ
	finish=clock();
	duration = (double)(finish - start) / CLOCKS_PER_SEC; 
	double fileLenMB=fileLen/(1024.0*1024.0);
	double speed=fileLenMB/duration;
	cout<<"fileLen: "<<fileLen<<" bytes     "<<"packCount: "<<packCount<<endl;
	cout<<"�ļ���С��"<<fileLen<<" bytes."<<endl;
	cout<<"�ļ���С "<<fileLenMB<<" MB"<<endl;
	cout<<"�����ļ� "<<fileName<<" ����ʱ "<<duration<<" sec.   ƽ���ٶ� "<<speed<<" MB/s."<<endl;
	system("pause");
	return 0;
}

#include <iostream>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <vector>
#include <algorithm>

#define C2S_PORT 12346
#define S2C_PORT 12347
#define BUF_SIZE 1024

using namespace std;

vector<char *> peer_list;

// client의 socket FD와 sockaddr 구조체 정보 저장 (pthread 인자로)
struct multiArgs {
    int sock_fd;
    sockaddr_in addr;
};

void printError(const char * message)
{
    cout << message << endl;
    exit(1);
}
 
void *newConnection(void * clientSock)
{
    // 
    int clientSocket = (*(multiArgs *)clientSock).sock_fd;
    // char buffer[BUF_SIZE] = {0, };
    sockaddr_in recv_from_client = (*(multiArgs *)clientSock).addr;     // rceive한 소켓에 명시된 addr 구조체

    // client addr 생성
    // int clnt_fd = socket(PF_INET, SOCK_STREAM, 0);
    // sockaddr_in client_addr;                                            // client_addr은 S2C port를 가지고 있는 구조체
    // memset(&client_addr, 0, sizeof(client_addr));
    // client_addr.sin_family = AF_INET;
    // client_addr.sin_port = htons(S2C_PORT);
    // // inet_pton(AF_INET, "192.168.0.104", &client_addr.sin_addr.s_addr);
    // client_addr.sin_addr.s_addr = recv_from_client.sin_addr.s_addr;

    // if(bind(clnt_fd, (sockaddr *)&client_addr, sizeof(sockaddr)))
    //     // printError("bind to client error");

    // // client로의 connect
    // if(connect(clnt_fd, (sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    //     printError("Connet to Client error");

    char *clnt_ip = inet_ntoa(recv_from_client.sin_addr);
    printf("Client Login : IP %s, Port %d\n", clnt_ip, ntohs(recv_from_client.sin_port));

    peer_list.push_back(clnt_ip);      // peer list에 client IP 추가
    
    char msg[BUF_SIZE];
    char send_msg[BUF_SIZE];
    int recv_len = recv(clientSocket, &msg, BUF_SIZE, 0);
    while(recv_len > 0) {
        printf("From Client : %s\n", msg);
        // send to client
        if(!strcmp(msg, "online_users")) {                          // client가 peer list를 요청
            cout << "Here in online_users\n";
            sprintf(msg, "%ld", peer_list.size());
            send(clientSocket, msg, BUF_SIZE, 0);

            for(int i = 0; i < peer_list.size(); i++) {
                recv(clientSocket, &msg, BUF_SIZE, 0);              // ack 수신
                send(clientSocket, peer_list[i], BUF_SIZE, 0);
            }
            recv(clientSocket, &msg, BUF_SIZE, 0);
            send(clientSocket, "list_end", BUF_SIZE, 0);
        }
        else if(!strcmp(msg, "logoff")) {
            auto iter = find(peer_list.begin(), peer_list.end(), clnt_ip); 
            if(iter == peer_list.end()) {
                cout << "IP not found" << endl;
                send(clientSocket, "IP not found", BUF_SIZE, 0);
            }
            else {
                peer_list.erase(iter);
                send(clientSocket, "Logout Success!", BUF_SIZE, 0);
                cout << "log out" << endl;
            }
        }

        recv_len = recv(clientSocket, &msg, BUF_SIZE, 0);
    }

}


int main(void) {
    int socketFd, clientSocket;
    sockaddr_in address, clientAddr;
    int addlen = sizeof(address);
    //소켓 설정
    socketFd = socket(PF_INET,SOCK_STREAM,0);
    if(socketFd == 0)
        printError("socket went wrong");
 
    //주소
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = 0;
    address.sin_port = htons(C2S_PORT);
 
    if(bind(socketFd, (sockaddr *)&address, addlen) < 0)
        printError("socket bind error");
 
    if(listen(socketFd, 1) < 0)
        printError("socket listen wrong");
    
    cout << "Start listening\n";
 
    pthread_t connThread;
    
    while(1) {
        clientSocket = accept(socketFd,(sockaddr *)&clientAddr, (socklen_t *)&addlen);
        if(clientSocket < 0)
        {
            printError("client socket went wrong");
        }
        cout << "Accepted" << endl;
        multiArgs pass_args = {clientSocket, clientAddr};

        // pthread_create(&connThread, NULL, newConnection, (void *)&clientSocket);
        pthread_create(&connThread, NULL, newConnection, (void*)&pass_args);
 
    }
 
    return 0;
}

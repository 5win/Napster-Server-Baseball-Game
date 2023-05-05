#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <vector>

using namespace std;

#define C2S_PORT 12346      // Client to Server (12345 -> 12346으로 port forwarding)
#define S2C_PORT 12347      // Server to Client
#define CC_PORT 12349       // Client to Client
#define BUF_SIZE 1024


// thread에 인자로 넣어줄 구조체
struct thread_args {
    int sock_fd;
    sockaddr_in addr;
};

int server_send_fd;
int server_recv_fd;
int serv_accept_fd;
sockaddr_in serv_addr, clnt_addr, serv_accept_addr;
vector<char *> peer_list;

void printError(string msg) {
    cout << msg << endl;
    exit(1);
}

// 서버로부터 응답을 받는 소켓 생성 및 대기
// void * recv_from_server(void * sock_info) {
//     int addrLen = sizeof(clnt_addr);

//     if(bind(server_recv_fd, (sockaddr *)&clnt_addr, addrLen) < 0) 
//         printError("Receive from server bind error");
    
//     if(listen(server_recv_fd, 3))
//         printError("Receive from server listen error");
//     cout << "Start receive from server" << endl;


//     // server로부터의 연결 수락
//     serv_accept_fd = accept(server_recv_fd, (sockaddr *)&serv_accept_addr, (socklen_t *)&addrLen);
//     if(serv_accept_fd < 0)
//         printError("Receive from server accept error");
//     cout << "Receive from server accepted\n";

//     printf("Server Info : IP %s, Port %d\n", inet_ntoa(serv_accept_addr.sin_addr), ntohs(serv_accept_addr.sin_port));

//     char buffer[BUF_SIZE];
//     int recv_len = recv(serv_accept_fd, &buffer, BUF_SIZE, 0);
//     while(recv_len > 0) {
//         if(strcmp(buffer, "list_end")) {               // list가 끝이면
//             cout << buffer << endl;
//         }
//         recv_len = recv(serv_accept_fd, &buffer, BUF_SIZE, 0);
//     }

//     // close(server_recv_fd);
//     cout << "thread end" << endl;
// }

void printMenu() {
    cout << "<Command list>\n";
    cout << "1. online_users\n";
    cout << "2. connect [ip] [port]\n";
    cout << "3. disconnect [peer]\n";
    cout << "4. guess [peer] [your guessing number]\n";
    cout << "5. answer [peer] [answer to the guess]\n";
    cout << "6. logoff\n";
}

void getOnlineUser() {
    char buffer[BUF_SIZE];
    send(server_send_fd, "online_users", BUF_SIZE, 0);      // server에게 online_users라는 문자열를 보냄
    recv(server_send_fd, &buffer, BUF_SIZE, 0);             // list의 길이를 반환받음
    int list_len;
    sscanf(buffer, "%d", &list_len);

    for(int i = 0; i < list_len; i++) {                 // list + list_end 개수 만큼 수신
        send(server_send_fd, "ack", BUF_SIZE, 0);       // send, receive 의 sync를 위한 ack
        recv(server_send_fd, &buffer, BUF_SIZE, 0);
        cout << buffer << endl;
    }
    send(server_send_fd, "ack", BUF_SIZE, 0);
    recv(server_send_fd, &buffer, BUF_SIZE, 0);
    cout << buffer << endl;
}

void logOff() {
    char buffer[BUF_SIZE];
    
    send(server_send_fd, "logoff", BUF_SIZE, 0);
    recv(server_send_fd, &buffer, BUF_SIZE, 0);
    cout << buffer << endl;
}

// 사용자 입력에 따른 동작
void menu() {
    string user_input, arg1, arg2;
    char buffer[BUF_SIZE];

    while(1) {
        cout << "Input Command : ";
        cin >> user_input;
        if(user_input == "help") {
            printMenu();
        }
        else if(user_input == "online_users") {                     // server에게 send하여 peer list 받아옴
            getOnlineUser();
        }
        else if(user_input == "connect") {
            cin >> arg1 >> arg2;

        }
        else if(user_input == "disconnect") {
            cin >> arg1;

        }
        else if(user_input == "guess") {
            cin >> arg1 >> arg2;
        }
        else if(user_input == "answer") {
            cin >> arg1 >> arg2;
        }
        else if(user_input == "logoff") {
            logOff();
        }
        cout << '\n';
    }
}

int main(void) {

    // send to server socket, addr_struct
    server_send_fd = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(C2S_PORT);
    inet_pton(AF_INET, "192.168.0.103", &serv_addr.sin_addr.s_addr);
    // serv_addr.sin_addr.s_addr = 0;          // server addr

    // recv from server socket, addr_struct
    // server_recv_fd = socket(PF_INET, SOCK_STREAM, 0);
    // memset(&clnt_addr, 0, sizeof(clnt_addr));
    // clnt_addr.sin_family = AF_INET;
    // clnt_addr.sin_port = htons(S2C_PORT);
    // // inet_pton(AF_INET, "192.16", &serv_addr.sin_addr.s_addr);
    // clnt_addr.sin_addr.s_addr = 0;          // my(client) addr

    // Create recv from server thread
    pthread_t from_other_clnt;
    // thread_args pass_to_recv_from_server = {server_recv_fd, clnt_addr};
    // pthread_create(&from_server, NULL, recv_from_server, (void *)&pass_to_recv_from_server);
    // pthread_create(&from_other_clnt, NULL, recv_from_server, NULL);

    // login
    bind(server_send_fd, (sockaddr*)&serv_addr, sizeof(sockaddr));
        // printError("Server bind error");

    if(connect(server_send_fd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        printError("Connect to Server error");
    cout << "login success!\n";

    menu();


    close(server_send_fd);


    return 0;
} 

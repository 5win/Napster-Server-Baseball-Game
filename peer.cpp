#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <vector>
#include <map>
#include <cstdlib>
#include <ctime>

using namespace std;

#define C2S_PORT 12346      // Client to Server
#define CC_PORT 12349       // Client to Client
#define BUF_SIZE 1024


// thread에 인자로 넣어줄 구조체
struct thread_args {
    int sock_fd;
    sockaddr_in addr;
};

struct thread_args_ip_port {
    char *ip;
    int port;
};

// global variables
int server_send_fd;             // regiServer 연결소켓
int p2p_recv_fd;                // p2p에서 서버역할 소켓
sockaddr_in serv_addr;          // regiServer 소켓 정보
sockaddr_in p2p_recv_addr;      // 나의 소켓 정보
vector<char *> peer_list;       // 서버에서 받아온 peer list
map<string, int> connected_peer;            // 현재 연결되어 있는 peer ip의 fd
map<string, int> game_try_cnt;              // 야구 게임 시도 횟수
map<string, string> peer_last_guess;        // 해당 peer의 최근 응답
map<string, string> baseball_answer;        // 각 peer가 맞춰야할 정답 
map<string, pthread_t> thread_list;         // connect한 thread list


void printError(string msg) {
    cout << msg << endl;
    exit(1);
}

string baseball_game(char *ip) {
    string last_guess = peer_last_guess[ip];
    string answer = baseball_answer[ip];
    bool a_check[3] = {0, };
    int strike_cnt = 0, ball_cnt = 0;

    for(int g = 0; g < 3; g++) {
        for(int a = 0; a < 3; a++) {
            if(a_check[a]) continue;
            if(g == a && last_guess[g] == answer[a]) {
                strike_cnt++;
                a_check[a] = true;
                break;
            }
            else if(last_guess[g] == answer[a]) {
                ball_cnt++;
                a_check[a] = true;
            }
        }
    }
    string ret;
    ret.push_back(strike_cnt + '0');
    ret.push_back(ball_cnt + '0');
    return ret;
}


// connect를 시도하는 peer에서 받는 peer로 연결한 thread
// answer는 여기서 수신해야함
void *send_to_clnt_thread(void *other_clnt_info) {
    char *ip = (*(thread_args_ip_port *)other_clnt_info).ip;               // 그냥 =으로 대입은 안되나?
    int port = (*(thread_args_ip_port *)other_clnt_info).port;
    int send_clnt_fd = socket(PF_INET, SOCK_STREAM, 0);
    sockaddr_in send_clnt_addr;

    memset(&send_clnt_addr, 0, sizeof(send_clnt_addr));
    send_clnt_addr.sin_family = AF_INET;
    send_clnt_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &send_clnt_addr.sin_addr.s_addr);

    bind(send_clnt_fd, (sockaddr *)&send_clnt_addr, sizeof(sockaddr));
    if(connect(send_clnt_fd, (sockaddr*)&send_clnt_addr, sizeof(sockaddr)) < 0)
        printError("Connect to other peer error");
    cout << "Connect to other peer success!" << endl;
    
    connected_peer[ip] = send_clnt_fd;                                      // 만약 이미 연결중이라면 거부하는 것 구현하기

    // 이 함수가 끝나도 fd로 send, recv 가능 -> thread는 종료되지 않음?

    char buffer[BUF_SIZE];
    int recv_len = recv(send_clnt_fd, &buffer, BUF_SIZE, 0);
    while(recv_len > 0) {
        if(!strcmp(buffer, "answer")) {
            send(send_clnt_fd, "ack", BUF_SIZE, 0);
            recv(send_clnt_fd, &buffer, BUF_SIZE, 0);
            if(!strcmp(buffer, "win")) {
                cout << "Correct!   " << game_try_cnt[ip] << " try\n\n";
                game_try_cnt[ip] = 0;                                       // 시도 횟수 초기화
                // 여기서 끝내나 아니면 랜덤숫자를 바꿔서 계속 하나?
            }
            else
                cout << "Answer From " << ip << " : "  << buffer[0] << " Strike, " << buffer[1] << " Ball" << endl;
        }
        else if(!strcmp(buffer, "disconnect")) {
            cout << "disconnect success!\n";
        }
        recv_len = recv(send_clnt_fd, &buffer, BUF_SIZE, 0);
    }
}

// 매 연결마다 독립적으로 생성될 thread (local variable만)
// 상대가 연결을 끊을 때는 thread 종료, main flow에서 
void *recv_from_clnt_thread(void * other_clnt_info) {
    int other_clnt_fd = (*(thread_args *)other_clnt_info).sock_fd;
    sockaddr_in other_clnt_addr = (*(thread_args *)other_clnt_info).addr;
    char buffer[BUF_SIZE];
    char *other_ip = inet_ntoa(other_clnt_addr.sin_addr);

    int recv_len = recv(other_clnt_fd, &buffer, BUF_SIZE, 0);
    while(recv_len > 0) {
        if(!strcmp(buffer, "disconnect")) {
            baseball_answer.erase(other_ip);
            connected_peer.erase(other_ip);
            send(other_clnt_fd, "disconnect", BUF_SIZE, 0);
            cout << "Disconnected from " << other_ip << endl;

            // socket, thread close, erase from thread list
            close(other_clnt_fd);
            pthread_cancel(thread_list[other_ip]);
            thread_list.erase(other_ip);
            game_try_cnt.erase(other_ip);
            break;      
        }
        else if(!strcmp(buffer, "guess")) {
            send(other_clnt_fd, "ack", BUF_SIZE, 0);
            recv(other_clnt_fd, &buffer, BUF_SIZE, 0);
            send(other_clnt_fd, "ack", BUF_SIZE, 0);
            cout << "Guess From " << other_ip << " : " << buffer << endl;

            peer_last_guess[other_ip] = buffer;                           // 해당 ip의 최근 guess number 저장
        }
        recv_len = recv(other_clnt_fd, &buffer, BUF_SIZE, 0);
    }
}

// 다른 peer에서 연결을 해오는 소켓 listening thread
// 1 개만 생성될 thread (전역 변수 사용 가능)
void *listen_from_clnt_thread(void *arg) {
    p2p_recv_fd = socket(PF_INET, SOCK_STREAM, 0);
    memset(&p2p_recv_addr, 0, sizeof(p2p_recv_addr));
    p2p_recv_addr.sin_family = AF_INET;
    p2p_recv_addr.sin_port = htons(CC_PORT);
    p2p_recv_addr.sin_addr.s_addr = 0;          // my(client) addr

    int addr_size = sizeof(p2p_recv_addr);
    if(bind(p2p_recv_fd, (sockaddr *)&p2p_recv_addr, addr_size) < 0) 
        printError("p2p recv bind error");
    if(listen(p2p_recv_fd, 1) < 0)
        printError("p2p recv listen error");

    pthread_t recv_other_clnt;
    sockaddr_in other_clnt_addr;
    int other_fd;

    while(1) {
        other_fd = accept(p2p_recv_fd, (sockaddr *)&other_clnt_addr, (socklen_t *)&addr_size);
        if(other_fd < 0)
            printError("Accept from other client error");
        cout << "Accepted from other clnt" << endl;

        char *other_ip = inet_ntoa(other_clnt_addr.sin_addr);
        srand(static_cast<unsigned int>(std::time(0)));
        string ans;
        for(int i = 0; i < 3; i++) 
            ans.push_back(rand() % 10 + '0');   
        // cout << "rand Num : " << ans << endl;

        baseball_answer[other_ip] = ans;                                                // 야구게임 답을 저장
        connected_peer[other_ip] = other_fd;                                            // 현재 연결된 peer의 fd 저장

        thread_args pass_args = {other_fd, other_clnt_addr};
        pthread_create(&recv_other_clnt, NULL, recv_from_clnt_thread, (void *)&pass_args);
    }
}

void printMenu() {
    cout << "\n<Command list>\n";
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

    for(int i = 0; i < list_len; i++) {                     // list + list_end 개수 만큼 수신
        send(server_send_fd, "ack", BUF_SIZE, 0);           // send, receive 의 sync를 위한 ack
        recv(server_send_fd, &buffer, BUF_SIZE, 0);
        cout << "IP : " << buffer << ", ";
        send(server_send_fd, "ack", BUF_SIZE, 0);
        recv(server_send_fd, &buffer, BUF_SIZE, 0);
        cout << "Port : " << buffer << endl;
    }
}

void connect(char *ip, int port) {
    if(connected_peer.find(ip) != connected_peer.end()) {
        cout << ip << " already connected\n";
        return;
    }
    pthread_t send_other_clnt;
    thread_args_ip_port pass_args = {ip, port};
    // cout << pass_args.ip << ", " << port << endl;
    pthread_create(&send_other_clnt, NULL, send_to_clnt_thread, (void *)&pass_args);
    
    thread_list[ip] = send_other_clnt;                       // thread descriptor 저장
    game_try_cnt[ip] = 0;                                    // 야구 게임 시도 횟수 초기화

    sleep(1);                                                // 반환되어버리면 구조체 내의 값들이 해제되어 사라짐
}

void disconnect(char *ip) {
    if(connected_peer.find(ip) == connected_peer.end()) {
        cout << ip << " not connected\n";
        return;
    }
    char buffer[BUF_SIZE];
    int opponent_fd = connected_peer[ip];
    connected_peer.erase(ip);
    send(opponent_fd, "disconnect", BUF_SIZE, 0);
}

void guess(char *ip, char *num) {
    if(connected_peer.find(ip) == connected_peer.end()) {
        cout << ip << " not connected\n";
        return;
    }
    char buffer[BUF_SIZE];
    int opponent_fd = connected_peer[ip];
    send(opponent_fd, "guess", BUF_SIZE, 0);
    send(opponent_fd, num, BUF_SIZE, 0);
    game_try_cnt[ip]++;                                     // 시도 횟수 +1
}

void answer(char *ip) {
    if(connected_peer.find(ip) == connected_peer.end()) {
        cout << ip << " not connected\n";
        return;
    }
    char buffer[BUF_SIZE];
    int opponent_fd = connected_peer[ip];

    string ans = baseball_game(ip);

    send(opponent_fd, "answer", BUF_SIZE, 0);
    if(ans == "30")     send(opponent_fd, "win", BUF_SIZE, 0);          // 정답일 때
    else                send(opponent_fd, ans.c_str(), BUF_SIZE, 0);
}

void logOff() {
    char buffer[BUF_SIZE];
    
    send(server_send_fd, "logoff", BUF_SIZE, 0);
    recv(server_send_fd, &buffer, BUF_SIZE, 0);
    cout << buffer << endl;

    exit(1);
}

// 사용자 입력에 따른 동작
void menu() {
    char user_input[BUF_SIZE], arg1[BUF_SIZE], arg2[BUF_SIZE];
    char buffer[BUF_SIZE];

    while(1) {
        cout << "---## Input Command ##---\n";
        cin >> user_input;

        // for(auto iter = connected_peer.begin(); iter != connected_peer.end(); iter++) {
        //     cout << iter->first << ", " << iter->second << endl;
        // }
        // for(auto iter = baseball_answer.begin(); iter != baseball_answer.end(); iter++)
        //     cout << iter->first << ", " << iter->second << endl;

        if(!strcmp(user_input, "help")) {
            printMenu();
        }
        else if(!strcmp(user_input, "online_users")) {                     // server에게 send하여 peer list 받아옴
            getOnlineUser();
        }
        else if(!strcmp(user_input, "connect")) {
            cin >> arg1 >> arg2;
            int tmp;
            sscanf(arg2, "%d", &tmp);
            connect(arg1, tmp);
        }
        else if(!strcmp(user_input, "disconnect")) {
            cin >> arg1;
            disconnect(arg1);
        }
        else if(!strcmp(user_input, "guess")) {
            cin >> arg1 >> arg2;
            guess(arg1, arg2);
        }
        else if(!strcmp(user_input, "answer")) {
            cin >> arg1;
            answer(arg1);
        }
        else if(!strcmp(user_input, "logoff")) {
            logOff();
        }
        cout << '\n';
        cin.clear();
    }
}

int main(void) {
    // send to server socket, addr_struct
    server_send_fd = socket(PF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(C2S_PORT);
    inet_pton(AF_INET, "192.168.0.103", &serv_addr.sin_addr.s_addr);

    pthread_t listen_other_clnt;
    pthread_create(&listen_other_clnt, NULL, listen_from_clnt_thread, NULL);

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

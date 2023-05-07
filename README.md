# Napster-Server-Baseball-Game

## Environment
- C++ , POSIX pthread
- x86_64
- ubuntu 20.04.5 LTS
- regiServer's Port : 12346
- peer's Port : 12349
- 하나의 router의 내부 IP로 서로 연결된 devices에서 실험
    - server-to-peer, peer-to-peer 모두 192.168.xxx.xxx의 내부 IP로 연결함
    - 외부 IP를 통한 Connection의 경우
        - regiServer와 peer가 같은 router 아래에서 같은 외부IP를 공유하며 존재할 때, peer에서 regiServer로 외부IP를 통해 연결하면 socket으로 전달된 peer's IP가 router's IP인 192.168.0.1로 전달되어 regiServer에서 peer's IP를 알 수 없는 문제 발생
        - 따라서, regiServer, peers 모두 별도의 독립적인 외부 IP를 가지며 port forwarding이 되어 있는 환경에서만 수행할 수 있었음



## Options
1. help
3. online_users                     
4. connect [peer ip] [port]     
5. disconnect [peer ip]
6. guess [peer ip] [predict number]
7. answer [peer ip]
8. logoff

### help
- Command list를 출력

### online_users
- regiServer에게 요청을 보내면, online 상태의 peer list를 받아서 출력

### connect [peer ip] [port]
- 숫자 야구를 신청할 상대의 ip와 port를 명시하면 P2P connection으로 숫자 야구 시작
- 이 때, 신청을 받은 peer는 3-digit random number를 생성

### disconnect [peer ip]
- 게임을 신청 받은 상대의 ip를 명시하여 게임 종료
- 종료 후, 두 peer에서의 상대방 ip와 생성한 3-digit random number를 삭제

### guess [peer ip] [predict number]
- 게임을 신청 받은 peer의 ip와 함께 예측 숫자를 상대에게 전송
- 숫자를 한 번 전송할 때 마다 game_try_cnt를 증가시켜 시도 횟수를 갱신
- 숫자를 받은 peer는 해당 숫자를 map 자료구조에 저장
- 받을 때 마다 새 값을 갱신

### answer [peer ip]
- 함수를 통해 연산된 예측 숫자에 대한 숫자 야구 결과 값을 전송
- 3 strike이면 "win"을 전송. 이 때, 신청한 peer에서는 game_try_cnt를 0으로 다시 초기화 시킴
- 아니라면 strike, ball 개수를 전송

### logoff
- regiServer에게 peer의 log out을 알림
- regiServer는 online peer list에서 해당 peer의 주소, 포트를 삭제
- 마지막으로 peer 프로그램을 종료




## Architecture

### regiServer
- 각 peer들과 연결될 때마다 새 thread를 생성하여 관리

### peer
- 먼저 regiServer와 연결
- 다른 peer로 부터 connection을 listening하는 listening thread를 생성
- listening thread에서 accept시 해당 P2P 통신을 수행하는 새 thread 생성
- main thread는 user I/O로 명령어를 받아서 수행




## Baseball Game

### Limits
- 두 peer가 서로 연결된 상태이면 새로 연결할 수 없음
- 예측 숫자는 반드시 3-digit number를 입력해야 함
- regiServer, peer 프로그램 종료 후, 다시 실행 시 binding error가 발생한다면, 해당 port의 TIME_WAIT가 끝나지 않은 것이므로 대기했다가 다시 실행해야 함

### Play
1. src peer에서 online_user로 online인 peer들을 확인
2. connect [ip] [prot]로 게임 시작
3. src peer는 guess [ip] [predict number]로 숫자 예측
4. dst peer가 guess를 받으면, answer [ip]로 결과 전송
5. src peer는 disconnect [ip]를 통해 게임 종료
6. logoff로 peer 프로그램 




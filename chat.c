#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>

// epoll ver.
#include <sys/epoll.h>
#define MAX_EVENTS 10


#define IP "127.0.0.1"
#define PORT 3001
#define MAX_CLIENT 1024
#define MAX_DATA 1024



static struct termios term_old;
void initTermios(void);
void resetTermios(void);

int launch_chat(void);
int launch_clients(int num_client);
int launch_server(void);
int get_server_status(void);
 
int
main(int argc, char *argv[])
{
    int ret = -1;
    int num_client;

    if ((argc != 2) && (argc != 3)) {
usage:  fprintf(stderr, "usage: %s a|m|s|c num_client\n", argv[0]);
        goto leave;
    }
    if ((strlen(argv[1]) != 1))
        goto usage;
    switch (argv[1][0]) {
      case 'a': if (argc != 3)
                    goto usage;
                if (sscanf(argv[2], "%d", &num_client) != 1)
                    goto usage;
                // Launch Automatic Clients
                ret = launch_clients(num_client);
                break;
      case 's': // Launch Server
                ret = launch_server();
                break;
      case 'm': // Read Server Status
                ret = get_server_status();
                break;
      case 'c': // Start_Interactive Chatting Session
                ret = launch_chat();
                break;
      default:
                goto usage;
    }
leave:
    return ret;
}

int
launch_chat(void)
{
    int clientSock;
    struct sockaddr_in serverAddr;
    fd_set rfds, wfds, efds;
    int ret = -1;
    char rdata[MAX_DATA];
    int i = 1;
    struct timeval tm;

    //epoll
    struct epoll_event ev, events[MAX_EVENTS];
    int listen_sock, conn_sock, nfds, epollfd;
    int j;
    //=epoll


    if ((ret = clientSock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        goto leave;
    }
    serverAddr.sin_family = AF_INET; // IPv4
    serverAddr.sin_addr.s_addr = inet_addr(IP); // 내가 연결할 서버의 IP
    serverAddr.sin_port = htons(PORT); // 내가 연결할 서버의 포트번호

    if ((ret = connect(clientSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)))) {
        perror("connect");
        goto leave1;
    }
    printf("[CLIENT] Connected to %s\n", inet_ntoa(*(struct in_addr *)&serverAddr.sin_addr));

    initTermios();

    // start select version of chatting ...
    i = 1;


    //대체 어디다가 쓰는건지 모르겠네
    //non blocking mode set: http://ozt88.tistory.com/20
    //ioctl(0, FIONBIO, (unsigned long *)&i);
    /*if ((ret = ioctl(clientSock, FIONBIO, (unsigned long *)&i))) {
        perror("ioctlsocket");
        goto leave1;
    }*/

    epollfd = epoll_create(10); //등록할 fd 최대 개수
    if (epollfd == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    //epoll: 관심있는 fd(clientSock)등록

    //ev.events  = EPOLLIN | EPOLLOUT | EPOLLERR; (EPOLLET 엣지트리거..?)
    ev.events = EPOLLIN;
    ev.data.fd = clientSock;
    //EPOLL_CTL_ADD <-> EPOLL_CTL_DEL
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientSock, &ev) == -1) {
        perror("epoll_ctl: clientSock");
        exit(EXIT_FAILURE);
    }

    //키보드 입력 등록(0)
    ev.events = EPOLLIN;
    ev.data.fd = 0;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, 0, &ev) == -1) {
        perror("epoll_ctl: stdin");
        exit(EXIT_FAILURE);
    }

    tm.tv_sec = 0; tm.tv_usec = 1000; // tv_usec: 1000마이크로 초. 즉, 1밀리초
    while (1) {

        //int  epoll_wait(int  epfd,  struct epoll_event * events, int maxevents, int timeout);
        //1. epollfd, 2. set된 event들, 3. 받아들일 최대 event개수, 
        //4. timeout: -1: 계속 기다림(blocking), 0: 바로 리턴(non blocking)
        if ((nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) == -1) {
            perror("epoll_pwait");
            exit(EXIT_FAILURE);
        }

        //epoll
        for(j=0; j<nfds; j++){ 
            //1. 서버로 부터 메시지가 온 경우
            if (events[j].data.fd == clientSock){
                if (!(ret = recv(clientSock, rdata, MAX_DATA, 0))) {
                printf("Connection closed by remote host\n");
                goto leave1;
                } 
                else if (ret > 0) {
                    for (i = 0; i < ret; i++) {
                        printf("%c", rdata[i]);
                    }
                    fflush(stdout);
                } 
                else
                    break;
            }
            //2. 키보드 입력 된 경우(stdin)
            else if(events[j].data.fd == 0){
                int ch = getchar();
                if ((ret = send(clientSock, &ch, 1, 0)) < 0)
                    goto leave1;
            }
        }
    }
leave1:
    resetTermios();
    close(clientSock);
leave:
    return -1;
}

int
launch_server(void)
{
    int serverSock, acceptedSock[20];
    int num_accepted = 0;
    struct sockaddr_in Addr;
    
    //epoll
    struct epoll_event ev, events[MAX_EVENTS];
    int nfds, epollfd;

    struct timeval tm;

    char data[MAX_DATA], *p;
    int ret, count, i = 1;

    int j;


    socklen_t AddrSize = sizeof(Addr);

    //SOCK_STREAM: TCP
    //PF_INET: IPv4 인터넷 프로토콜을 사용해 통신
    if ((ret = serverSock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        goto leave;
    }

    //에러로 인해 종료된 프로그램의 소켓에서 해당 포트를 물고 있으면 bind()에러가 남
    //그걸 방지해주기 위해 사용
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (void *)&i, sizeof(i));

    Addr.sin_family = AF_INET; //IPv4 인터넷 프로토콜 주소체계
    Addr.sin_addr.s_addr = INADDR_ANY; //내 컴퓨터의 아무 네트워크에서 받아들임
    Addr.sin_port = htons(PORT); // #define PORT 3000
    if ((ret = bind(serverSock, (struct sockaddr *)&Addr,sizeof(Addr)))) {
        perror("bind");
        goto error;
    }

    //listen(fd, 최대 연결을 기다릴 큐의 길이: 보통5)
    if ((ret = listen(serverSock, 5))) {
        perror("listen");
        goto error;
    }

    epollfd = epoll_create(10); //등록할 fd 최대 개수
    if (epollfd == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    //epoll: 관심있는 fd(clientSock)등록

    //serverSock 등록
    //ev.events  = EPOLLIN | EPOLLOUT | EPOLLERR; (EPOLLET 엣지트리거..?)
    ev.events = EPOLLIN;
    ev.data.fd = serverSock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverSock, &ev) == -1) {
        perror("epoll_ctl: serverSock");
        exit(EXIT_FAILURE);
    }

    tm.tv_sec = 0; tm.tv_usec = 1000; // tv_usec: 1000마이크로 초. 즉, 1밀리초
    while (1) {
        //int  epoll_wait(int  epfd,  struct epoll_event * events, int maxevents, int timeout);
        //1. epollfd, 2. set된 event들, 3. 받아들일 최대 event개수, 
        //4. timeout: -1: 계속 기다림(blocking), 0: 바로 리턴(non blocking)
        if ((nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) == -1) {
            perror("epoll_pwait");
            exit(EXIT_FAILURE);
        }

        //epoll
        for(j=0; j<nfds; j++){ 
            //1. serverSock으로 새로운 클라이언트 요청이 왔을 경우
            if (events[j].data.fd == serverSock){
                if ((acceptedSock[num_accepted++] = accept(serverSock, (struct sockaddr*)&Addr, &AddrSize)) < 0) {
                    perror("accept");
                    ret = -1;
                    goto error;
                }

                //acceptedSock 등록
                ev.events = EPOLLIN;
                ev.data.fd = acceptedSock[num_accepted-1];
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, acceptedSock[num_accepted-1], &ev) == -1) {
                    perror("epoll_ctl: acceptedSock");
                    exit(EXIT_FAILURE);
                }
                printf("[SERVER] Connected to %s\n", inet_ntoa(*(struct in_addr *)&Addr.sin_addr));
            }
            //2. 연결된 클라이언트로 부터 메시지가 왔을 경우 : else로 해도 되나...?
            else{
                //recv 마지막 0은 flag: 상관x
                if (!(ret = count = recv(events[j].data.fd, data, MAX_DATA, 0))) {
                    fprintf(stderr, "Connect Closed by Client\n");
                    break;
                }
                if (ret < 0) {
                    perror("recv");
                    break;
                }

                //1. 먼저 나한테 출력하고
                for (i = 0; i < count; i++)
                    printf("%c", data[i]);
                fflush(stdout);
                //출력 버퍼에 있는 값들 바로 출력. 이렇게 해줘야 바로바로 출력됨.

                //2. 모든 클라이언트에 메시지 쏴줌
                for(j=0; j<num_accepted; j++){
                    int temp_count = count;
                    p = data;
                    while (temp_count) {
                        if ((ret = send(acceptedSock[j], p, temp_count, 0)) < 0) {
                            perror("send");
                            close(acceptedSock[j]);
                            break;
                        }
                        temp_count -= ret; //혹시 메시지가 덜 send됐으면, 남은 메시지도 이어서 쭉 보내줌
                        p = p + ret;
                    }
                }
            }
        }
    }
    for(j=0; j<num_accepted; j++){
        close(acceptedSock[j]);
    }
error:
    close(serverSock);
leave:
    return ret;
}

int
launch_clients(int num_client)
{
    return 0;
}

int
get_server_status(void)
{
    return 0;
}

/* Initialize new terminal i/o settings */
void
initTermios(void) 
{
    struct termios term_new;

    tcgetattr(0, &term_old); /* grab old terminal i/o settings */
    term_new = term_old; /* make new settings same as old settings */

    //Canonical and noncanonical mode
    //http://man7.org/linux/man-pages/man3/termios.3.html
    term_new.c_lflag &= ~ICANON; /* disable buffered i/o */

    term_new.c_lflag &= ~ECHO;   /* set no echo mode */
    tcsetattr(0, TCSANOW, &term_new); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void
resetTermios(void) 
{
    tcsetattr(0, TCSANOW, &term_old);
}

﻿#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>

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

    tm.tv_sec = 0; tm.tv_usec = 1000; // 1000마이크로 초. 즉, 1밀리초
    while (1) {
        FD_ZERO(&rfds); FD_ZERO(&wfds); FD_ZERO(&efds);
        //FD_SET(clientSock, &wfds);
        FD_SET(clientSock, &rfds);
        FD_SET(clientSock, &efds);
        FD_SET(0, &rfds);

        //rfds: 읽을 것이 있는지 확인할 목록
        //wfds: 쓸 것이 있는지 확인할 목록
        //efds: 예외사항이 있는지 확인할 목록. 에러가 있는지?
        //tm: 기다릴 시간. NULL이면 무한정 기다림. 0이면 바로 리턴. 얘도 어디다가 쓰는건지 모르겠네
        if ((ret = select(clientSock + 1, &rfds, &wfds, &efds, &tm)) < 0) {
            perror("select");
            goto leave1;
        } else if (!ret)	// nothing happened within tm
            continue; //다시 기다림
        if (FD_ISSET(clientSock, &efds)) {
            printf("Connection closed\n");
            goto leave1;
        }
        if (FD_ISSET(clientSock, &rfds)) {
            if (!(ret = recv(clientSock, rdata, MAX_DATA, 0))) {
                printf("Connection closed by remote host\n");
                goto leave1;
            } else if (ret > 0) {
                for (i = 0; i < ret; i++) {
                    printf("%c", rdata[i]);
                }
                fflush(stdout);
            } else
                break;
        }
        if (FD_ISSET(0, &rfds)) {
            //int ch = getchar();
            char ch;
            scanf("%c", &ch);
            if ((ret = send(clientSock, &ch, 1, 0)) < 0)
                goto leave1;
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
    int serverSock, acceptedSock;
    struct sockaddr_in Addr;
    socklen_t AddrSize = sizeof(Addr);

    char data[MAX_DATA], *p;
    int ret, count, i = 1;

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
    if ((ret = listen(serverSock, 1))) {
        perror("listen");
        goto error;
    }

    if ((acceptedSock = accept(serverSock, (struct sockaddr*)&Addr, &AddrSize)) < 0) {
        perror("accept");
        ret = -1;
        goto error;
    }

    //inet_ntoa: little endian <-> big endian 변환?
    printf("[SERVER] Connected to %s\n", inet_ntoa(*(struct in_addr *)&Addr.sin_addr));
    //close(serverSock);

    while (1) {
        //recv 마지막 0은 flag: 상관x
        if (!(ret = count = recv(acceptedSock, data, MAX_DATA, 0))) {
            fprintf(stderr, "Connect Closed by Client\n");
            break;
        }
        if (ret < 0) {
            perror("recv");
            break;
        }
        //printf("[%d]", count); fflush(stdout);

        //1. 먼저 나한테 출력하고
        for (i = 0; i < count; i++)
            printf("%c", data[i]);
        fflush(stdout);
        //출력 버퍼에 있는 값들 바로 출력. 이렇게 해줘야 바로바로 출력됨.

        p = data;

        //2. 클라이언트에 메시지 쏴줌
        while (count) {
            if ((ret = send(acceptedSock, p, count, 0)) < 0) {
                perror("send");
                break;
            }
            count -= ret; //혹시 메시지가 덜 send됐으면, 남은 메시지도 이어서 쭉 보내줌
            p = p + ret;
        }
    }

    close(acceptedSock);
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

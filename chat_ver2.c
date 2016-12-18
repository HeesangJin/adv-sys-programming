#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <pthread.h>
//ssh test

// epoll ver.
#include <sys/epoll.h>
#define MAX_EVENTS 10


#define IP "127.0.0.1"
#define PORT 3001
#define MAX_CLIENT 1024
#define MAX_DATA 2048

#define TOTAL_CLIENT 20


static struct termios term_old;

void *server_init(void *param);
void *client_init(void *cindex);

int launch_chat(int cindex);
int launch_clients(int num_client);
int launch_server(void);
int launch_program(int);

int
main(int argc, char *argv[])
{
    int ret = -1;
    int num_client;
    struct timeval before, after; //측정 시간 전,후
    int duration;

    gettimeofday(&before, NULL); //시간 측정 종료

    if ((argc != 2) && (argc != 3)) {
usage:  fprintf(stderr, "usage: %s a|m|s|c num_client\n", argv[0]);
        goto leave;
    }
    if ((strlen(argv[1]) != 1))
        goto usage;
    switch (argv[1][0]) {
      case 'a': // Launch Automatic Clients
                ret = launch_program(TOTAL_CLIENT);
                break;
      case 's': // Launch Server
                ret = launch_server();
                break;
      case 'c': // Start_Interactive Chatting Session
                ret = launch_chat(atoi(argv[2]));
                break;
      default:
                goto usage;
    }

    gettimeofday(&after, NULL); //시간 측정 종료
    
    duration = (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_usec - before.tv_usec);
    printf("end process\n");
    printf("Processing time = %d.%06d sec\n", duration / 1000000, duration % 1000000);

leave:
    return ret;
}

int
launch_chat(int cindex)
{
    int clientSock;
    struct sockaddr_in serverAddr;
    fd_set rfds, wfds, efds;
    int ret = -1;
    char rdata[MAX_DATA], *p;
    char file_name[15];
    int i = 1;
    struct timeval tm;
    int start_read = 0, end_read = 0, end_program = 0;

    //epoll
    struct epoll_event ev, events[MAX_EVENTS];
    int listen_sock, conn_sock, nfds, epollfd;
    int j;
    //=epoll

    FILE *file_in, *file_out;

    sprintf(file_name, "/tmp/file_%.4d",cindex);
    file_in = fopen(file_name,"r");
    if (file_in == NULL) perror ("Error opening file");

    sprintf(file_name, "output_%.4d",cindex);
    file_out = fopen(file_name,"w");
    if (file_out == NULL) perror ("Error opening file");

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
    //printf("[CLIENT] Connected to %s\n", inet_ntoa(*(struct in_addr *)&serverAddr.sin_addr));

    //initTermios();

    // start select version of chatting ...
    i = 1;

    epollfd = epoll_create(5); //등록할 fd 최대 개수
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

    tm.tv_sec = 0; tm.tv_usec = 1000; // tv_usec: 1000마이크로 초. 즉, 1밀리초
    while (1) {
        if(end_program == 1)
            break;

        if(start_read == 1 && end_read == 0){
            if(fgets(rdata, MAX_DATA, file_in)){
                int temp_count = strlen(rdata);
                p = rdata;
                while (temp_count) {
                    if ((ret = send(clientSock, rdata, temp_count, 0)) < 0) {
                        perror("send");
                        close(clientSock);
                        break;
                    }
                    temp_count -= ret; //혹시 메시지가 덜 send됐으면, 남은 메시지도 이어서 쭉 보내줌
                    p = p + ret;
                }
            }
            //파일을 다 읽었으면 보내기는 끝
            else{
                int ch = '@';
                if ((ret = send(clientSock, &ch, 1, 0)) < 0) {
                    perror("send");
                    close(clientSock);
                    break;
                }
                fclose(file_in);
                end_read = 1;
            }
        }
        //int  epoll_wait(int  epfd,  struct epoll_event * events, int maxevents, int timeout);
        //1. epollfd, 2. set된 event들, 3. 받아들일 최대 event개수, 
        //4. timeout: -1: 계속 기다림(blocking), 0: 바로 리턴(non blocking)
        if ((nfds = epoll_wait(epollfd, events, MAX_EVENTS, 0)) == -1) {
            perror("epoll_pwait");
            exit(EXIT_FAILURE);
        }

        //epoll
        for(j=0; j<nfds; j++){ 
            //1. 서버로 부터 메시지가 온 경우
            if (events[j].data.fd == clientSock){
                int k;
                int remain;
                char *p;
                if (!(remain = ret = recv(clientSock, rdata, MAX_DATA, 0))) {
                    printf("Connection closed by remote host\n");
                    goto leave1;
                }
                p = rdata;
                remain=0;
                // $가 오면 파일 읽어서 보내기 시작
                for (k = 0; k < ret; k++){
                    //심볼들이 오면 무시(파일에 안씀)

                    if(rdata[k]=='$'){
                        start_read = 1;
                    }
                    else if(rdata[k]=='%'){
                        end_program = 1;
                    }

                    if(rdata[k]=='@' || rdata[k]=='$' || rdata[k]=='%'){
                        fwrite(p, 1, remain, file_out); //이 전 위치까지 파일에 쓰고(이전까지: remain개)
                        p = &rdata[k] + 1; //다음 위치 부터 다시 카운트
                        remain=0;
                        continue;
                    }
                    remain++;
                }
                fwrite( p, 1, remain, file_out);
                
            }
        }
    }
leave1:
    //resetTermios();
    close(clientSock);
    fclose(file_out);
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
    int num_end=0;
    int num_disconnect=0;

    int end_server = 0;

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
    if ((ret = listen(serverSock, 20))) {
        perror("listen");
        goto error;
    }

    epollfd = epoll_create(20); //등록할 fd 최대 개수
    if (epollfd == -1) {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    //1. 클라이언트 20(TOTAL_CLIENT)개 연결 받기
    ev.events = EPOLLIN;
    ev.data.fd = serverSock;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, serverSock, &ev) == -1) {
        perror("epoll_ctl: serverSock");
        exit(EXIT_FAILURE);
    }

    while (num_accepted<TOTAL_CLIENT) {
        //블럭킹 모드로 클라이언트 연결 받기
        if ((nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) == -1) {
            perror("epoll_pwait");
            exit(EXIT_FAILURE);
        }
        for(j=0; j<nfds; j++){ 
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
            //printf("[SERVER] Connected to %s\n", inet_ntoa(*(struct in_addr *)&Addr.sin_addr));
        }
    }

    //2. 모든 클라이언트에게 $송신(파일 달라고 요청)
    for(j=0; j<num_accepted; j++){
        int ch = '$';
        if ((ret = send(acceptedSock[j], &ch, 1, 0)) < 0)
            goto error;
    }
    printf("%d clients are connected\n",num_accepted);

    printf("start process...\n");
    while (1) {
        //모든 클라이언트가 연결을 끊었으면 종료
        if (num_disconnect == num_accepted)
            break;

        //@ 20개 수신 -> 모든 클라이언트에게 %송신
        if(end_server == 0&& num_end == num_accepted){
            for(j=0; j<num_accepted; j++){
                int ch = '%';
                if ((ret = send(acceptedSock[j], &ch, 1, 0)) < 0) {
                    perror("send");
                    close(acceptedSock[j]);
                    break;
                }
            }
            end_server = 1;
        }

        //int  epoll_wait(int  epfd,  struct epoll_event * events, int maxevents, int timeout);
        //1. epollfd, 2. set된 event들, 3. 받아들일 최대 event개수, 
        //4. timeout: -1: 계속 기다림(blocking), 0: 바로 리턴(non blocking)
        if ((nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) == -1) {
            perror("epoll_pwait");
            exit(EXIT_FAILURE);
        }

        //epoll
        for(j=0; j<nfds; j++){ 
            //데이터 받고: 데이터 받는 건 non-blocking 으로 받기 때문에 while을 쓰지 않음
            //1. 클라이언트쪽으로 부터 연결 종료 됨
            if (!(ret = count = recv(events[j].data.fd, data, MAX_DATA, 0))) {
                //fprintf(stderr, "Connect Closed by Client\n");
                num_disconnect++;

                //epoll 삭제
                ev.events = EPOLLIN;
                ev.data.fd = events[j].data.fd;
                if (epoll_ctl(epollfd, EPOLL_CTL_DEL, events[j].data.fd, &ev) == -1) {
                    perror("epoll_ctl: events[j].data.fd");
                    exit(EXIT_FAILURE);
                }
                break;
            }

            //2. 에러
            if (ret < 0) {
                perror("recv");
                break;
            }

            //3. 일반적인 메시지 받음 -> 바로 모든 클라이언트에게 송신
            else{
                for (i = 0; i < count; i++)
                    if(data[i]=='@'){
                        num_end++;
                    }
                
                //모든 클라이언트에 메시지 쏴줌
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
launch_program(int num_client)
{
    pthread_t p_sthread, p_cthread[TOTAL_CLIENT]; //thread ID 서버, 클라이언트 아이디
    int thr_id; //thread generation error check
    int status; //thread 종료시 반환하는 값 저장 변수
    int i, cindex[TOTAL_CLIENT];

    
    thr_id = pthread_create(&p_sthread, NULL, server_init, NULL);
    if(thr_id < 0)
    {
        perror("thread create error : ");
        exit(0);
    }
    

    for(i=0; i<TOTAL_CLIENT; i++ ){
        cindex[i] = i+1;
        thr_id = pthread_create(&p_cthread[i], NULL, client_init, (void *)&cindex[i]);
        if(thr_id < 0)
        {
                perror("thread create error : ");
                exit(0);
        }
    }

    for(i=0; i<TOTAL_CLIENT; i++ ){
        pthread_join(p_cthread[i], (void **)&status);
    }

    pthread_join(p_sthread, (void **)&status);
    return 0;
}

void *client_init(void *cindex)
{
    launch_chat(*((int*)cindex));
}
void *server_init(void *param)
{
    launch_server();   
}

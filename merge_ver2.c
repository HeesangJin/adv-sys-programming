#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FILESIZE 1048576 // 1048576 = 1024*1024 =1MB

//no reverse

//1048576: 1Mega
int
main(int argc, char *argv[])
{
    //FILE *fdout; //각 파일의 파일 구조체
    char *file1, *file2, *fileout, *temp_out; //mmap사용할때 사용
    char *pfile1, *pfile2, *pfileout, *cfile; // file1, file2 mmap 사용할 시 pointer

    int fd1, fd2, fdout; //file descriptor: 항상 필요함
    int eof1 = 0, eof2 = 0; //eof(파일 끝일 경우 -1)
    long line1 = 0, line2 = 0, lineout = 0; 
    struct timeval before, after; //측정 시간 전,후
    int duration; //지속시간
    int ret = 1; //리턴
    int flag = PROT_WRITE | PROT_READ;
    int i=0;
    int rsize1, rsize2, *csize; // file1 , file2 의 남은 size와 current size pointer 
    int currentFile; // file1 or file2 선택

    char buffer[1024], *pbuffer, *pbufferfin; // to reverse
    int sbuf, sbuffin; 

    if (argc != 4) { //실행 파라미터를 모두 안씀 
        fprintf(stderr, "usage: %s file1 file2 fout\n", argv[0]);
        goto leave0;
    }
    if ((fd1 = open(argv[1], O_RDWR|O_CREAT)) < 0) { //file1 이 없으면 에러
        perror(argv[1]);
        goto leave0;
    }
    if ((fd2 = open(argv[2], O_RDWR|O_CREAT)) < 0) { //file2 이 없으면 에러
        perror(argv[2]);
        goto leave1;
    }
    if ((fdout = open(argv[3], O_RDWR|O_CREAT)) < 0) { //file1 이 없으면??? 에러
        perror(argv[3]);
        goto leave2;
    }
    //////////////////////////////////// 파일 메모리 맵핑
    if((file1 = mmap(0, FILESIZE , flag, MAP_SHARED,fd1,0)) == NULL){
        perror("mmap error");
        exit(1);
    }

    if((file2 = mmap(0, FILESIZE , flag, MAP_SHARED,fd2,0)) == NULL){
        perror("mmap error");
        exit(1);
    }


    if((fileout = mmap(0, FILESIZE * 2 , flag, MAP_SHARED, fdout, 0)) == NULL){
        perror("mmap error");
        exit(1);
    }
    //////////////////////////////////// //파일 메모리 맵핑

    gettimeofday(&before, NULL); //시간 측정 시작

    // initialize;
    pfile1 = file1;
    pfile2 = file2;
    pfileout = fileout;
    pbuffer = buffer;

    rsize1 = FILESIZE;
    rsize2 = FILESIZE;
    sbuf=0;
    sbuffin=0;
    currentFile = 1;

    while(rsize1 && rsize2){
        if(currentFile == 1){
            rsize1--;
            sbuf++;
            if((*pfileout++ = *pfile1++) == '\n'){
                currentFile=2;
            }
        }
        else if(currentFile == 2){
            rsize2--;
            sbuf++;
            if((*pfileout++ = *pfile2++) == '\n'){
                currentFile=1;
            }
        }
    }

    while(rsize1){
        *pfileout++ = *pfile1++;
        rsize1--;
    }
    while(rsize2){
        *pfileout++ = *pfile2++;
        rsize2--;
    }

    gettimeofday(&after, NULL); //시간 측정 종료
    
    //=====결과 출력(여긴 그대로 냅두면 될듯)
    duration = (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_usec - before.tv_usec);
    printf("Processing time = %d.%06d sec\n", duration / 1000000, duration % 1000000);
    printf("File1 = %ld, File2= %ld, Total = %ld Lines\n", line1, line2, lineout);
    ret = 0;
    //=====
    
leave3:
    munmap(fileout,FILESIZE * 2); // 1024 x 1024 = 1048576 = 1Mega
leave2:
    munmap(file1,FILESIZE); // 1048576: 1Mega
    munmap(file2,FILESIZE); // 1048576: 1Mega
leave1:
    //munmap(file2,5242880);
leave0:
    close(fdout);
    close(fd1);
    return ret; 
}

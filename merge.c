#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

// 1048576 = 1024*1024 =1MB

//완성
int
main(int argc, char *argv[])
{
    int ret = 1; //리턴
    long line1 = 0, line2 = 0, lineout = 0; //line count
    struct timeval before, after; //측정 시간 전,후
    int duration;

    int fd1, fd2, fdout; //file descriptor: 항상 필요함
    char *file1, *file2, *fileout, *temp_out; //mmap사용할때 사용
    char *pfile1, *pfile2, *pfileout, *cfile; // file1, file2 mmap 사용할 시 pointer
    int rsize1, rsize2, *csize; // file1 , file2 의 남은 size
    int currentFile; // file1 or file2 선택

    int flag = PROT_WRITE | PROT_READ;
    char *head_line, *tail_line, *index_line; // to reverse
    FILE *fpMe;
    int filelen1, filelen2;

    //file size check
    if((fpMe = fopen(argv[1],"rb")) == NULL){
        perror(argv[1]);
        goto leave0;
    }
    fseek(fpMe, 0L, SEEK_END);
    filelen1 = ftell(fpMe);
    fclose(fpMe);

    if((fpMe = fopen(argv[2],"rb")) == NULL){
        perror(argv[2]);
        goto leave0;
    }
    fseek(fpMe, 0L, SEEK_END);
    filelen2 = ftell(fpMe);
    fclose(fpMe);

    //file descriptor
    if (argc != 4) {
        fprintf(stderr, "usage: %s file1 file2 fout\n", argv[0]);
        goto leave0;
    }
    if ((fd1 = open(argv[1], O_RDWR|O_CREAT)) < 0) {
        perror(argv[1]);
        goto leave0;
    }
    if ((fd2 = open(argv[2], O_RDWR|O_CREAT)) < 0) {
        perror(argv[2]);
        goto leave1;
    }
    if ((fdout = open(argv[3], O_RDWR|O_CREAT,0644)) < 0) {
        perror(argv[3]);
        goto leave2;
    }
    write(fdout, NULL, 0);
    ftruncate(fdout, filelen1 + filelen2);

    //file - memory mapping
    if((file1 = mmap(0, filelen1 , flag, MAP_SHARED,fd1,0)) == NULL){
        perror("mmap error");
        goto leave3;
    }
    if((file2 = mmap(0, filelen2 , flag, MAP_SHARED,fd2,0)) == NULL){
        perror("mmap error");
        goto leave4;
    }
    if((fileout = mmap(0, filelen1 + filelen2 , flag, MAP_SHARED, fdout, 0)) == NULL){
        perror("mmap error");
        goto leave5;
    }

    gettimeofday(&before, NULL); //시간 측정 시작

    // initialize;
    pfile1 = file1; pfile2 = file2;
    pfileout = fileout;

    rsize1 = filelen1; rsize2 = filelen2;
    currentFile = 1;

    head_line = pfile1;
    while(rsize1 && rsize2){
        if(currentFile == 1){
            rsize1--;
            if((*pfile1++) == '\n'){
                line1++; currentFile=2;
                tail_line = pfile1; index_line = --tail_line; //현재 위치를 잡고, tail은 '\n'을 가르킴
                while(1){  //마지막껀 \n이므로 제외해야함
                    *pfileout++ = *(--index_line); //index는 '\n'보다 하나 전부터 index를 하나씩 뒤로 미루면서
                    if(index_line==head_line){ //라인의 첫번째 인덱스면
                        *pfileout++ = *tail_line; //'\n'을 넣음
                        head_line = pfile2; //file1이 끝났으므로 file2의 다음 위치를 head로 잡음
                        break;
                    }
                }
            }
        }
        else if(currentFile == 2){
            rsize2--;
            if((*pfile2++) == '\n'){
                line2++; currentFile=1;
                tail_line = pfile2; index_line = --tail_line; //현재 위치를 잡고, tail은 '\n'을 가르킴
                while(1){  //마지막껀 \n이므로 제외해야함
                    *pfileout++ = *(--index_line); //index는 '\n'보다 하나 전부터 index를 하나씩 뒤로 미루면서
                    if(index_line==head_line){ //라인의 첫번째 인덱스면
                        *pfileout++ = *tail_line; //'\n'을 넣음
                        head_line = pfile1; //file2이 끝났으므로 file1의 다음 위치를 head로 잡음
                        break;
                    }
                }
            }
        }
    }

    while(rsize1){
        rsize1--;
        if((*pfile1++) == '\n'){
            line1++;
            tail_line = pfile1; index_line = --tail_line; //현재 위치를 잡고, tail은 '\n'을 가르킴
            while(1){  //마지막껀 \n이므로 제외해야함
                *pfileout++ = *(--index_line); //index는 '\n'보다 하나 전부터 index를 하나씩 뒤로 미루면서
                if(index_line==head_line){ //라인의 첫번째 인덱스면
                    *pfileout++ = *tail_line; //'\n'을 넣음
                    head_line = pfile1; //다음 위치를 head로 잡음
                    break;
                }
            }
        }
    }
    while(rsize2){
        rsize2--;
        if((*pfile2++) == '\n'){
            line2++;
            tail_line = pfile2; index_line = --tail_line; //현재 위치를 잡고, tail은 '\n'을 가르킴
            while(1){  //마지막껀 \n이므로 제외해야함
                *pfileout++ = *(--index_line); //index는 '\n'보다 하나 전부터 index를 하나씩 뒤로 미루면서
                if(index_line==head_line){ //라인의 첫번째 인덱스면
                    *pfileout++ = *tail_line; //'\n'을 넣음
                    head_line = pfile2; //다음 위치를 head로 잡음
                    break;
                }
            }
        }
    }

    gettimeofday(&after, NULL); //시간 측정 종료
    
    lineout = line1 + line2;
    //=====결과 출력(여긴 그대로 냅두면 될듯)
    duration = (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_usec - before.tv_usec);
    printf("Processing time = %d.%06d sec\n", duration / 1000000, duration % 1000000);
    printf("File1 = %ld, File2= %ld, Total = %ld Lines\n", line1, line2, lineout);
    ret = 0;
    //=====
leave6:
    munmap(fileout,filelen1 + filelen2);
leave5:
    munmap(file2,filelen2);
leave4: 
    munmap(file1,filelen1);
leave3:
    close(fdout);
leave2:
    close(fd2);
leave1:
    close(fd1);
leave0:
    return ret; 
}
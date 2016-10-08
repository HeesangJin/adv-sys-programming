#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
//ver2 : open() & mmap()
// fopen() 대신 open()사용
// fgetc() 대신 mmap()사용
// fputc() 대신 흠...
int readaline_and_out(FILE *fin, FILE *fout);

int
main(int argc, char *argv[])
{
    //FILE *fdout; //각 파일의 파일 구조체
    char *file1, *file2, *fileout;
    int fd1, fd2, fdout;
    int eof1 = 0, eof2 = 0; //eof(파일 끝일 경우 -1)
    long line1 = 0, line2 = 0, lineout = 0; 
    struct timeval before, after; //측정 시간 전,후
    int duration; //지속시간
    int ret = 1; //리턴
    int flag = PROT_WRITE | PROT_READ;
    int i=0;

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
    
    //fallocate(fdout, 0, 0, 1048576*16);

    if((file1 = mmap(0, 104857600 , flag, MAP_SHARED,fd1,0)) == -1){
        perror("mmap error");
        exit(1);
    }

    if((fileout = mmap(0, 104857600 * 2 , flag, MAP_SHARED, fdout, 0)) == -1){
        perror("mmap error");
        exit(1);
    }

    gettimeofday(&before, NULL); //시간 측정 시작

    /*
    while(1){
        if(file1[i] == '\0')
            break;
        fileout[i] = file1[i];
        i++;
    }    
    */
    sprintf(fileout, "%s", file1);

    /*
    //==============Core
    do {
        //readaline_and_out 리턴값이 
        //1이면: if(0)-> else: 문자 끝에 도달하였다. 이제 readaline_and_out()안함
        //0이면: if(1) : 문자끝에 도달하지 않았으므로, 라인 개수를 증가시킨다.(line++, lineout++)
        //

        if (!eof1) {
            if (!readaline_and_out(file1, fout)) { //param: input1, result 
                line1++; lineout++;
            } else
                eof1 = 1; //이제 이 파일은 검사안함
        }
        if (!eof2) {
            if (!readaline_and_out(file2, fout)) { //param: input2, result
                line2++; lineout++;
            } else
                eof2 = 1;
        }
    } while (!eof1 || !eof2); //두 파일 모두 읽을 때까지 반복
    //==============//Core
    */
    gettimeofday(&after, NULL); //시간 측정 종료
    
    //=====결과 출력(여긴 그대로 냅두면 될듯)
    duration = (after.tv_sec - before.tv_sec) * 1000000 + (after.tv_usec - before.tv_usec);
    printf("Processing time = %d.%06d sec\n", duration / 1000000, duration % 1000000);
    printf("File1 = %ld, File2= %ld, Total = %ld Lines\n", line1, line2, lineout);
    ret = 0;
    //=====
    
leave3:
    munmap(fileout,104857600 * 2); // 1048576: 1Mega
leave2:
    munmap(file1,104857600); // 1048576: 1Mega
leave1:
    //munmap(file2,5242880);
leave0:
    close(fdout);
    close(fd1);
    return ret; 
}

/* Read a line from fin and write it to fout */
/* return 1 if fin meets end of file */
int //fgetc, fputc는 자리 쓰면 다음 포인터로 알아서 넘어가는 듯
readaline_and_out(FILE *fin, FILE *fout)
{    
    //ch: fgetc로 넣어오는 문자. (일반 문자 또는 eof문자)
    //count: 문자를 몇개 읽었는지 확인. 0인지 아닌지만 필요한듯.
    int ch, count = 0;

    do {
        if ((ch = fgetc(fin)) == EOF) { //파일 끝이면
            //파일 끝인데 아예 없으면, 줄바꿈도 쓰면 안됨. 
            //즉, file1은 다쓰고 file2만 남았으면, file1은 \n도 쓰면 안되기 때문에 넣은듯
            if (!count) //(이것도 각 파일마다 한번만 실행)
                return 1; //카운트 된 줄이 없으면 리턴1
            else {
                fputc(0x0a, fout); // 파일 끝이면, 줄바꿈 문자를 씀(각 파일마다 한번만 실행될듯)
                break; //카운트 된 줄이 하나 있으므로 리턴0
            }
        }
        fputc(ch, fout);
        count++;
    } while (ch != 0x0a); //줄바꿈 라인이면 그만둠
    return 0;
}


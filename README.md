# 고급 시스템 프로그래밍 Advanced System Programming

국민대학교 컴퓨터공학부

- 실습을 위한 파일들
- 예제 파일들
 
File

- gen.c   : n 개의  m MB의 크기의 text 파일 만들기 (file merge용)
- merge.c : merge.c 두개의 파일을 merge 하고 시간을 재는 예제 소스 
- chat.c  : chatting을 위한 소스 (아직 준비 안됨)

```
$ make
```
```
$ ./gen 2 100
```
```
$ ls -l /tmp/file_*
```
```
$ ./merge /tmp/file_0001 /tmp/file_0002 f_out
```
```
$ ls -l f_out
```
```
$ mkfile 10m 10Mega
```

## malloc lab(CMU 버전) 구현(C언어)
### 세팅 방법
```
sudo apt update
sudo apt install build-essential
sudo apt install gdb
sudo apt-get install gcc-multilib g++-multilib

git clone https://github.com/seanlion/malloc_lab_implementation.git

program 폴더에서 make clean
make
```

### 파일 설명
```
밖에 있는 c파일들은 동일한 내용을 다른 버전으로 구현한 것.
program 폴더에 있는 mm.c 파일의 내용을 갈아끼워넣으면 됨.

mm.c : 수정해야 할 유일한 파일(implicit list로first_fit으로 구현한 버전)(54점)
mm_next_fit.c : implicit list 버전에서 next_fit으로 탐색한 버전(84점)
mm_explicit.c : explicit list과 first fit으로 구현한 버전(87점)

/traces : 테스트 꾸러미 폴더
mdriver.c : 테스트 꾸러미 및 기타 라이브러리를 실행해서 코드를 채점하는 프로그램
그 외 : mm.c 및 mdriver.c에 필요한 파일들. 만지지 말 것
```

### 실행하기
```
program/detail/mdriver : 채점하기

program/detail/mdriver -f traces/binary2-bal.rep : 특정 테스트 세트로 채점
```

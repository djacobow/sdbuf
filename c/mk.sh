rm -f *.o test1 test2

CFLAGS="-g -Og -Wall -fsanitize=memory -fno-omit-frame-pointer"
LDFLAGS="--stdlib=libc++ -rdynamic"
clang $CFLAGS -c debug.c -o debug.o
clang $CFLAGS -c sdbuf.c -o sdbuf.o
clang $CFLAGS -c test_example_1.c -o test_example_1.o
clang $CFLAGS -rdynamic debug.o sdbuf.o test_example_1.o -o test1
./test1

clang++ $CFLAGS -std=c++11 -stdlib=libc++ -c test_example_2.cpp -o test_example_2.o
clang++ $CFLAGS -std=c++11 -stdlib=libc++ sdbuf.o test_example_2.o debug.o -o test2
./test2


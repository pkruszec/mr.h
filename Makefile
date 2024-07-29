CC = clang
EXENAME = test.exe

$(EXENAME): test.c
	$(CC) -g -o $@ $^


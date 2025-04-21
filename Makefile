main: main.c
	$(CC) main.c -o main -Wall -Wextra -pedantic -std=c99 -lm -lncursesw
mousetest: mousetest.c
	$(CC) mousetest.c -o mousetest -Wall -Wextra -pedantic -std=c99 -lm -lncursesw
test2: test2.c
	$(CC) test2.c -o test2 -Wall -Wextra -pedantic -std=c99 -lm -lncursesw


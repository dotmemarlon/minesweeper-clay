CC=cc

all: main
	@echo 'Build successfully'

main: main.c
	$(CC) main.c -o main -Wall `pkg-config --libs --cflags sdl3 sdl3-ttf sdl3-image` -lm

debug: main.c
	$(CC) -fsanitize=address -ggdb main.c -o debug -Wall `pkg-config --libs --cflags sdl3 sdl3-ttf sdl3-image` -lm

release: main.c
	$(CC) main.c -o release -Ofast `pkg-config --libs --cflags sdl3 sdl3-ttf sdl3-image` -lm

run: main
	./main

clean:
	rm main

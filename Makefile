all:
	 gcc -g src/fs.c src/utility.c src/interface.c `pkg-config fuse --cflags --libs` -o ./fs -lm
test: 
	./fs

clean:
	rm ./fs 

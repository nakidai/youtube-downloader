all: downloader
downloader: downloader.c
clean: downloader
	rm -f clock
.PHONY: all clean

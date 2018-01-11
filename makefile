LXMLSRC=lxml2
OUT=fnotify
.PHONY: all master lxml

all : master lxml 
master : main.o
	gcc -o $(OUT) main.o -lgumbo `pkg-config --libs libcurl`
lxml : $(LXMLSRC).o
	gcc -o $(OUT)_lxml $(LXMLSRC).o -lxml2 `pkg-config --libs libcurl libnotify`
main.o : main.c
	gcc -c main.c -g `pkg-config --cflags libcurl`
$(LXMLSRC).o : $(LXMLSRC).c
	gcc -c $(LXMLSRC).c -g `pkg-config --cflags libcurl libxml-2.0 libnotify`


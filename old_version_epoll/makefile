CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g 

TARGET = server
# TARGET = client
OBJS = ./*.cc

all: $(OBJS)
	$(CXX) $(CFLAGS) main.cc echo.cc echo.h ikcp.h ikcp.c -o $(TARGET)  
	# $(CXX) $(CFLAGS) Main.cc client.cc client.h ikcp.h ikcp.c -o $(TARGET)  
clean:
	rm -rf *.o server


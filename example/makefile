# Makefile for chat_server and chat_client programs

# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -pthread

TARGETS = chat_server chat_client
all: $(TARGETS)

# compile chat and server
chat_server: server.o
	$(CXX) $(CXXFLAGS) -o chat_server server.o
chat_client: client.o
	$(CXX) $(CXXFLAGS) -o chat_client client.o


# compile the server and client into object file
server.o: server.cpp
	$(CXX) $(CXXFLAGS) -c server.cpp
client.o: client.cpp
	$(CXX) $(CXXFLAGS) -c client.cpp


clean:
	rm -f *.o $(TARGETS)

# to run write make in the terminal

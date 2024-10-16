# Makefile for chat_server and chat_client programs

# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++11 -pthread
TARGETS = chat_server chat_client

all: $(TARGETS)

# Compile chat_server and chat_client directly from their source files
chat_server: server.cpp
	$(CXX) $(CXXFLAGS) -o chat_server server.cpp
chat_client: client.cpp
	$(CXX) $(CXXFLAGS) -o chat_client client.cpp

clean:
	rm -f $(TARGETS)

# to run, type "make" in the terminal

//
// Simple chat client for TSAM-409
//
// Command line: ./chat_client 127.0.0.1 4000
//
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <thread>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <ctime>



// Threaded function for handling responss from server
void listenServer(int serverSocket)
{
    int nread;                                  // Bytes read from socket
    char buffer[1025];                          // Buffer for reading input

    while(true)
    {
       memset(buffer, 0, sizeof(buffer));
       nread = read(serverSocket, buffer, sizeof(buffer));

       if(nread == 0)                      // Server has dropped us
       {
          printf("Over and Out\n");
          exit(0);
       }
       else if(nread > 0)
       {
          printf("%s\n", buffer);
       }
    }
}


// function for client to send command to server
void clientCommand(int serverSocket, std::string command) {
    //Parsing the command
    std::vector<std::string> tokens;
    std::stringstream stream(command);
    std::string token;

    // Split command from client into tokens for parsing
    while(std::getline(stream, token, ',')) {
        tokens.push_back(token);
    }


    if (tokens.size() < 2) {
        std::cerr << "Invalid command format." << std::endl;
        return;
    }

    //Printing the command
    std::cout << "Received command: " << tokens[0] << std::endl;


   // Check for command type
    if (tokens[0] == "GETMSG") {
        // format: GETMSG,GROUP ID
        printf("GETMSG command received\n");

        std::string groupId = tokens[1];
        
        std::string response = "secret_GETMSG," + groupId;
        send(serverSocket, response.c_str(), response.size(), 0);
        
        
    }
    else if (tokens[0] == "GET_MSG_RESPONSE") {
        printf("MSG_RESPONSE command from server received\n");
        // format: MSG_RESPONSE,MESSAGE
        //Response from server with message to my client

        //Trimming command from message and printing message
        std::string message = tokens[1];
        std::cout << "Message from server: " << message << std::endl;

    }
    else if (tokens[0] == "SENDMSG") {
        // format: SENDMSG,GROUP ID,<message contents> 
        // sending SENDMSG command to server for server to process
        printf("SENDMSG command received\n");
        std::string groupId = tokens[1];
        std::string message = tokens[2];
        std::string response = "secret_SENDMSG," + groupId + "," + message;

        send(serverSocket, response.c_str(), response.size(), 0);
        
    }
    else if (tokens[0] == "LISTSERVERS") {
        // format: LISTSERVERS
        printf("LISTSERVERS command received\n");
        std::string response = "secret_LISTSERVERS";
        send(serverSocket, response.c_str(), response.size(), 0);
    }
    else {
        std::string errorMsg = "ERROR client command not recognized\n";
    }
}


int main(int argc, char* argv[])
{
   struct addrinfo hints, *svr;              // Network host entry for server
   struct sockaddr_in serv_addr;           // Socket address for server
   int serverSocket;                         // Socket used for server 
   char buffer[1025];                        // buffer for writing to server
   bool finished;                   
   int set = 1;                              // Toggle for setsockopt

   if(argc != 3)
   {
        printf("Usage: chat_client <ip  port>\n");
        printf("Ctrl-C to terminate\n");
        exit(0);
   }

   hints.ai_family   = AF_INET;            // IPv4 only addresses
   hints.ai_socktype = SOCK_STREAM;

   memset(&hints,   0, sizeof(hints));

   if(getaddrinfo(argv[1], argv[2], &hints, &svr) != 0)
   {
       perror("getaddrinfo failed: ");
       exit(0);
   }

   struct hostent *server;
   server = gethostbyname(argv[1]);

   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   bcopy((char *)server->h_addr,
      (char *)&serv_addr.sin_addr.s_addr,
      server->h_length);
   serv_addr.sin_port = htons(atoi(argv[2]));

   serverSocket = socket(AF_INET, SOCK_STREAM, 0);

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
       printf("Failed to set SO_REUSEADDR for port %s\n", argv[2]);
       perror("setsockopt failed: ");
   }

   
   if(connect(serverSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr) )< 0)
   {
       // EINPROGRESS means that the connection is still being setup. Typically this
       // only occurs with non-blocking sockets. (The serverSocket above is explicitly
       // not in non-blocking mode, so this check here is just an example of how to
       // handle this properly.)
       if(errno != EINPROGRESS)
       {
         printf("Failed to open socket to server: %s\n", argv[1]);
         perror("Connect failed: ");
         exit(0);
       }
   }
   //handleHELO(serverSocket);
    // Listen and print replies from server
   std::thread serverThread(listenServer, serverSocket);

   finished = false;
   while(!finished)
   {
       bzero(buffer, sizeof(buffer));

        // Read input from stdin
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
            perror("fgets failed");
            break;
        }

        // Remove the trailing newline character from the input
        buffer[strcspn(buffer, "\n")] = 0;

        // Convert buffer to std::string
        std::string command(buffer);

        // Check for exit command
        if (command == "exit") {
            printf("Exiting client.\n");
            finished = true;
            break;
        }

        // Call clientCommand with the input command
        clientCommand(serverSocket, command);

   }
}


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
#include <list>
#include <netinet/in.h>  

#include <iostream>
#include <sstream>
#include <thread>
#include <map>
<<<<<<< Updated upstream:example/server.cpp
=======
#include <queue>

>>>>>>> Stashed changes:example/tsamgroup17.cpp
#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections
std::map<int, int> messagesWaiting; 

std::map<std::string, std::queue<std::string>> groupMessages;

std::vector<std::string> connectedServers;


int serverPort; // Global variable to store the server port


class Client
{
  public:
    int sock;              // socket of client connection
    std::string name;  // Limit length of name of client's user
    std::string ip_address;
    int port = serverPort; 
    std::string group_id;  // Group ID for the client

    Client(int socket, std::string ip, int port) : sock(socket), ip_address(ip), port(port) {} 

    ~Client(){}            // Virtual destructor defined for base class
};


class Server
{
public:
    std::string ip_address;
    int port = serverPort;
    std::string group_id;

    Server(std::string ip, int port, std::string group_id) : ip_address(ip), port(port), group_id(group_id) {}

    ~Server() {}
};

std::list<Server> connectedServers;
std::map<int, Client*> clients; // Lookup table for per Client information
std::map<std::string, Client*> groupClients; // Lookup table for group ID to Client

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
{
   struct sockaddr_in sk_addr;   // address settings for bind()
   int sock;                     // socket opened for this port
   int set = 1;                  // for setsockopt

   // Create socket for connection. Set to be non-blocking, so recv will
   // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__     
   if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("Failed to open socket");
      return(-1);
   }
#else
   if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
   {
     perror("Failed to open socket");
    return(-1);
   }
#endif

   // Turn on SO_REUSEADDR to allow socket to be quickly reused after 
   // program exit.

   if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
   {
      perror("Failed to set SO_REUSEADDR:");
   }
   set = 1;
#ifdef __APPLE__     
   if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
   {
     perror("Failed to set SOCK_NOBBLOCK");
   }
#endif
   memset(&sk_addr, 0, sizeof(sk_addr));

   sk_addr.sin_family      = AF_INET;
   sk_addr.sin_addr.s_addr = INADDR_ANY;
   sk_addr.sin_port        = htons(portno);

   // Bind to socket to listen for connections from clients

   if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
   {
      perror("Failed to bind to socket:");
      return(-1);
   }
   else
   {
      printf("Server: Opened socket on port %d\n", portno);
      serverPort = portno; // Initialize the global variable
      return(sock);
   }
}


void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{
    
    // Print client information before closing the connection
    if (clients.find(clientSocket) != clients.end())
    {
        Client *client = clients[clientSocket];
        std::cout << "Closing connection for client: " << std::endl;
        std::cout << "Socket: " << client->sock << std::endl;
        std::cout << "IP Address: " << client->ip_address << std::endl;
        std::cout << "Port: " << client->port << std::endl;
        std::cout << "Group ID: " << client->group_id << std::endl;
    }
    else
    {
        std::cerr << "Error: Client not found in clients map." << std::endl;
        return;
    }

    //Removing from groupClients map if it exists
    if (!clients[clientSocket]->group_id.empty())
    {
        groupClients.erase(clients[clientSocket]->group_id);
    }

    printf("Client closed connection: %d\n", clientSocket);

    // If this client's socket is maxfds then the next lowest
    // one has to be determined. Socket fd's can be reused by the Kernel,
    // so there aren't any nice ways to do this.

    close(clientSocket);

    if (*maxfds == clientSocket)
    {
        *maxfds = -1;
        for (auto const &p : clients)
        {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
    }

    // And remove from the list of open sockets.

    FD_CLR(clientSocket, openSockets);

    // Remove from groupClients map if it exists
    if (clients[clientSocket]->group_id != "")
    {
        groupClients.erase(clients[clientSocket]->group_id);
    }

    // Remove from clients map
    delete clients[clientSocket]; // Free the memory allocated for the client
    clients.erase(clientSocket);
}

// Process command from client on the server
void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds, char *buffer)
{
    std::vector<std::string> tokens;
    std::string token;

    // Split command from client into tokens for parsing
    std::stringstream stream(buffer);

    while (std::getline(stream, token, ','))
        tokens.push_back(token);

    // Trim trailing newline characters from the last token
    if (!tokens.empty() && !tokens.back().empty() && tokens.back().back() == '\n')
    {
        tokens.back().pop_back();
    }

    // Print debugging output in the correct order
    std::cout << "Received command: " << tokens[0] << std::endl;
    std::cout << "Full command: " << buffer << std::endl;

    if ((tokens[0].compare("CONNECT") == 0) && (tokens.size() == 2))
    {
        clients[clientSocket]->name = tokens[1];
    }
    else if (tokens[0].compare("LEAVE") == 0)
    {
        std::cout << "Processing LEAVE command" << std::endl;
        closeClient(clientSocket, openSockets, maxfds);
    }
    else if (tokens[0].compare("WHO") == 0)
    {
        std::cout << "Who is logged on" << std::endl;
        std::string msg;

        for (auto const &names : clients)
        {
            msg += names.second->name + ",";
        }
        send(clientSocket, msg.c_str(), msg.length() - 1, 0);
    }
    else if ((tokens[0].compare("MSG") == 0) && (tokens[1].compare("ALL") == 0))
    {
        std::string msg;
        for (auto i = tokens.begin() + 2; i != tokens.end(); i++)
        {
            msg += *i + " ";
        }

        for (auto const &pair : clients)
        {
            send(pair.second->sock, msg.c_str(), msg.length(), 0);
        }
    }
    else if (tokens[0].compare("MSG") == 0)
    {
        for (auto const &pair : clients)
        {
            if (pair.second->name.compare(tokens[1]) == 0)
            {
                std::string msg;
                for (auto i = tokens.begin() + 2; i != tokens.end(); i++)
                {
                    msg += *i + " ";
                }
                send(pair.second->sock, msg.c_str(), msg.length(), 0);
            }
        }
    }
    else if (tokens[0].compare("HELO") == 0)
    {
        if (tokens.size() == 2)
        {
            std::string fromGroupId = tokens[1];
            clients[clientSocket]->group_id = fromGroupId;
            groupClients[fromGroupId] = clients[clientSocket];

<<<<<<< Updated upstream:example/server.cpp
            // Store the server information
            connectedServers.push_back(Server(clients[clientSocket]->ip_address, clients[clientSocket]->port, fromGroupId));
=======
            //Sending ACK to comfirm establishment
            std::string ack = "ACK,HELO," + fromGroupId;
            send(clientSocket, ack.c_str(), ack.size(), 0);
>>>>>>> Stashed changes:example/tsamgroup17.cpp

            // Construct the SERVERS response
            std::string response = "SERVERS";
            for (const auto &server : connectedServers)
            {
                response += "," + server.group_id + "," + server.ip_address + "," + std::to_string(serverPort) + ";";
            }
            send(clientSocket, response.c_str(), response.size(), 0);
        }
    }
<<<<<<< Updated upstream:example/server.cpp
    else if (tokens[0].compare("LISTSERVERS") == 0)
    {
        // Construct the SERVERS response
        std::string response = "SERVERS";
        for (const auto &server : connectedServers)
        {
            response += "," + server.group_id + "," + server.ip_address + "," + std::to_string(serverPort) + ";";
        }
        send(clientSocket, response.c_str(), response.size(), 0);
=======
    else if (tokens[0].compare("SENDMSG") == 0) {
        if(token.size() >= 3) {
            
            std::string groupId = tokens[1];

            std::string message = tokens[2];

            //Placeholder for later making sure message starts and ends with correct SOH...

            groupMessages[groupId].push(message);

            //send ACK?
        }
    }
    else if (tokens[0].compare("GETMSG") == 0) {
        //looks for messages in the group message queue 
        if(token.size() >= 2) {
            
            std::string groupId = tokens[1];
            
            //Check if there are message on queue
            if (groupMessages.find(groupId) != groupMessages.end() && !groupMessages[groupId].empty()) {
                
                //making sure sending the first message on queue
                std::string message = groupMessages[groupId].front();
                groupMessages[groupId].pop();

                //Sending message to the client
                std::string response = "MSG," + groupId + "," + message;
                send(clientSocket, response.c_str(), response.size(), 0);       //Might want to switch to a seperate function later
            }
            //No messages on queue
            else {
                std::string NoMessage = "NO_MSG," + groupId;
                send(clientSocket, NoMessage.c_str(), NoMessage.size(), 0); 
            }
        } 
        else {
            std::string errorMsg = "ERROR,GETMSG: GETMSG requires GroupID";
            send(clientSocket, errorMsg.c_str(), errorMsg.size(), 0);
        }
    
>>>>>>> Stashed changes:example/tsamgroup17.cpp
    }
    else
    {
        std::cout << "Unknown command from client:" << buffer << std::endl;
    }
}


int main(int argc, char *argv[])
{
    bool finished;
    int listenSock;     // Socket for connections to server
    int clientSock;     // Socket of connecting client
    fd_set openSockets; // Current open sockets
    fd_set readSockets; // Socket list for select()
    fd_set exceptSockets; // Exception socket list
    int maxfds;         // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[1025]; // buffer for reading from clients

    if (argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to

    listenSock = open_socket(atoi(argv[1]));
    printf("Listening on port: %d\n", atoi(argv[1]));

    if (listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }
    else
    // Add listen socket to socket set we are monitoring
    {
        FD_ZERO(&openSockets);
        FD_SET(listenSock, &openSockets);
        maxfds = listenSock;
    }

    finished = false;

    while (!finished)
    {
        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

        if (n < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            // First, accept  any new connections to the server on the listening socket
            if (FD_ISSET(listenSock, &readSockets))
            {
                clientLen = sizeof(client);
                clientSock = accept(listenSock, (struct sockaddr *)&client, &clientLen);
                printf("accept***\n");
                // Add new client to the list of open sockets
                FD_SET(clientSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = std::max(maxfds, clientSock);

                // Get client IP address and port
                std::string clientIp = inet_ntoa(client.sin_addr);
                int clientPort = ntohs(client.sin_port);

                // create a new client to store information.
                clients[clientSock] = new Client(clientSock, clientIp, clientPort);

                // Decrement the number of sockets waiting to be dealt with
                n--;

                printf("Client connected on server: %d\n", clientSock);
            }
            // Now check for commands from clients
            std::list<Client *> disconnectedClients;
            while (n-- > 0)
            {
                for (auto const &pair : clients)
                {
                    Client *client = pair.second;

                    if (FD_ISSET(client->sock, &readSockets))
                    {
                        // recv() == 0 means client has closed connection
                        if (recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                        {
                            disconnectedClients.push_back(client);
                            closeClient(client->sock, &openSockets, &maxfds);
                        }
                        // We don't check for -1 (nothing received) because select()
                        // only triggers if there is something on the socket for us.
                        else
                        {
                            clientCommand(client->sock, &openSockets, &maxfds, buffer);
                        }
                    }
                }
                // Remove client from the clients list
                for (auto const &c : disconnectedClients)
                    clients.erase(c->sock);
            }
        }
    }
}
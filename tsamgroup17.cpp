//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
//
// Code based on code from Jacky Mallett (jacky@ru.is)
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
#include <list>
#include <netinet/in.h>  

#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <unistd.h>
#include <queue>

#define SOH 0x01 // Start of Header (SOH)
#define EOT 0x04 // End of message (EOT)

//extern std::list<Server> connectedServers;
std::string serverGroupId;
std::string serverIpAddress;


// SOCK_NONBLOCK for OSX and mb linux ?
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif




#define BACKLOG  5          // Allowed length of queue of waiting connections
std::map<int, int> messagesWaiting; 
std::map<std::string, std::queue<std::string>> groupMessages;
int serverPort; // Global variable to store the server port



// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
  public:
    int sock;              // socket of client connection
    std::string name;  // Limit length of name of client's user
    std::string ip_address;
    int port; 
    std::string group_id;  // Group ID for the client

    Client(int socket, std::string ip, int port) : sock(socket), ip_address(ip), port(port) {} 

    ~Client(){}            // Virtual destructor defined for base class
};


class Server
{
public:
    std::string ip_address;
    int port;
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

// formatting and sending message
void sendMessage(int sock, const std::string &message) {
    std::string formattedMessage;
    formattedMessage += SOH;
    formattedMessage += message;
    formattedMessage += EOT;

    send(sock, formattedMessage.c_str(), formattedMessage.size(), 0);
}

//Creating the "SERVERS" command back to server
std::string createServersResponse() {
    std::string response = "SERVERS";

    // Adding to SERVERS COMMAND
    response += "," + serverGroupId + "," + serverIpAddress + "," + std::to_string(serverPort) + ";";

    // Add connected servers' information
    for (const auto &server : connectedServers) {
        response += server.group_id + "," + server.ip_address + "," + std::to_string(server.port) + ";";
    }

    return response;
}

void sendServersResponse(int serverSocket) {
    // Build the SERVERS response
    std::string response = createServersResponse();

    // Send the response
    sendMessage(serverSocket, response);
}


// Process command from Server to server
void serverCommand(int serverSocket, fd_set *openSockets, int *maxfds, char *buffer){
    //Removing SOH and EOT characters
    std::string message(buffer);
    if (message.front() == SOH) {
        message.erase(0, 1); // Remove SOH
    }
    if (message.back() == EOT) {
        message.pop_back(); // Remove EOT
    }

    std::vector<std::string> tokens;
    std::stringstream stream(message);
    std::string token;

    while (std::getline(stream, token, ',')) {
        tokens.push_back(token);
    }

    // Print debugging output in the correct order
    std::cout << "Received command: " << tokens[0] << std::endl;
    std::cout << "Full command: " << buffer << std::endl;

    //Hello command processing
    if (tokens[0] == "HELO" && tokens.size() == 2) {
        std::string fromGroupId = tokens[1];

        // Get other server's IP address and port
        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);
        getpeername(serverSocket, (struct sockaddr*)&peer_addr, &peer_addr_len);
        std::string fromIpAddress = inet_ntoa(peer_addr.sin_addr);
        int fromPort = ntohs(peer_addr.sin_port);

        // Store the connecting server's information
        connectedServers.push_back(Server(fromIpAddress, fromPort, fromGroupId));
        // Send the SERVERS response
        sendServersResponse(serverSocket);

    } else if (tokens[0] == "SERVERS") {
        printf("Received SERVERS command from server\n");
        // Send the SERVERS response
        // Collect information from the SERVERS command about connected servers
        // Add servers from the SERVERS command to the connectedServers list
        //Server command format: SERVERS,group_id,ip_address,port;group_id,ip_address,port;...
        for (size_t i = 1; i < tokens.size(); i += 3) {
            std::string group_id = tokens[i];
            std::string ip_address = tokens[i + 1];
            int port = std::stoi(tokens[i + 2]);

            connectedServers.push_back(Server(ip_address, port, group_id));
        }
    } else {
        std::cout << "Unknown command from server: " << buffer << std::endl;
    }


}

// Process command from client on the server
void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds, char *buffer)
{
    // Check if the message is from a server (contains SOH and EOT)
    if (buffer[0] == SOH) {
        // This is a server command
        serverCommand(clientSocket, openSockets, maxfds, buffer);
        return;
    }
    
    
    std::vector<std::string> tokens;
    std::string token;

    // Split command from client into tokens for parsing
    std::stringstream stream(buffer);

    while (std::getline(stream, token, ',')) {
        tokens.push_back(token);
    }

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
    else if ((tokens[0].compare("CONNECTTO") == 0) && (tokens.size() == 2))
    {   
        // client gives server port and asks server to connect to another server on the network
        std::cout << "Processing CONNECTTO command" << std::endl;
        
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

            // Store the server information
            connectedServers.push_back(Server(clients[clientSocket]->ip_address, clients[clientSocket]->port, fromGroupId));

            // Construct the SERVERS response
            std::string response = "Response from server: SERVERS";
            for (const auto &server : connectedServers)
            {
                response += "," + server.group_id + "," + server.ip_address + "," + std::to_string(serverPort) + ";";
            }
            send(clientSocket, response.c_str(), response.size(), 0);
        }
    }
    else if (tokens[0].compare("LISTSERVERS") == 0)
    {
        // Construct the SERVERS response
        std::string response = "Response from server: LISTSERVERS";
        for (const auto &server : connectedServers)
        {
            response += "," + server.group_id + "," + server.ip_address + "," + std::to_string(serverPort) + ";";
        }
        send(clientSocket, response.c_str(), response.size(), 0);
    }
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

    }
    else
    {
        std::cout << "Unknown command from client:" << buffer << std::endl;
    }
}
int connectToServer(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Cannot create socket");
        return -1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    return sock;
}

void sendHeloMessage(int sock) {
    std::string heloCommand = "HELO," + serverGroupId; // Construct HELO message with the server's group ID
    sendMessage(sock, heloCommand); // Send the HELO message using the existing sendMessage function
    std::cout << "Sent: " << heloCommand << std::endl;
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

    serverGroupId = "A5_17";
    serverIpAddress = "130.208.246.249";
    serverPort = atoi(argv[1]);

    // Setup socket for server to listen to

    listenSock = open_socket(serverPort);
    printf("Listening on port: %d\n", serverPort);

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


    // Connect to remote server 
    int remoteServerSock = connectToServer("130.208.246.249", 5001);
    std::cout << "remoteserversock:  " << std::to_string(remoteServerSock) << std::endl;
    if (remoteServerSock != -1) {
        FD_SET(remoteServerSock, &openSockets);
        maxfds = std::max(maxfds, remoteServerSock);
        printf("Connected to remote server\n");
        sendHeloMessage(remoteServerSock);
    }

    // while connected keep checking server command
    serverCommand(remoteServerSock, &openSockets, &maxfds, "Helo");
    // print connectedservers list


    // Create a new client object for the remote server
    Client *remoteServerClinet = new Client(remoteServerSock, "130.208.246.249", 5001);
    //add client1 to clinet map
    clients[remoteServerSock] = remoteServerClinet;


    
    // Main server loop - accept and handle client connections
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
                // SEND HELO HERE
                sendHeloMessage(remoteServerSock);
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
                        std:: cout << std:: to_string (client->sock) << std::endl;
                        // recv() == 0 means client has closed connection
                        if (recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                        {
                            // print buffer
                            printf("Buffer: %s\n", buffer);
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
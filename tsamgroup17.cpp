//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000 
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
#include <list>
#include <netinet/in.h>  
#include <ifaddrs.h> // for getting network interface addresses
#include <net/if.h>  // for base interface definitions
#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <unistd.h>
#include <queue>
#include <mutex>
#include <fstream>
#include <iostream>
#include <ctime>

#define SOH 0x01 // Start of Header (SOH)
#define EOT 0x04 // End of message (EOT)

//extern std::list<Server> connectedServers;
std::string serverGroupId;
std::string serverIpAddress;
std::mutex serverMutex;
std::string A17serverIpAddress;

// SOCK_NONBLOCK for OSX and mb linux ?
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG  5          // Allowed length of queue of waiting connections
std::ofstream logFile;
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

void initializeLogFile() {
    logFile.open("server_log.txt", std::ofstream::out | std::ofstream::app);
    if (!logFile.is_open()) {
        std::cerr << "Failed to open log file!" << std::endl;
        exit(1); // Handle error opening log file
    }
}
void logMessage(const std::string& message) {
    // Get current timestamp
    std::time_t now = std::time(nullptr);
    char timestamp[100];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    // Write to log file
    logFile << "[" << timestamp << "] " << message << std::endl;
}

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

// Close the client connection and remove the client from the list of open sockets
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
    }
    else
    {
        std::cerr << "Error: Client not found in clients map." << std::endl;
        logMessage("Error: Client not found in clients map.");
        return;
    }
    //Removing from groupClients map if it exists
    if (!clients[clientSocket]->group_id.empty())
    {
        groupClients.erase(clients[clientSocket]->group_id);
    }

    printf("Client closed connection: %d\n", clientSocket);
    logMessage("Client closed connection: " + std::to_string(clientSocket));

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

    //Make sure the message size does not exceed 5000 bytes
    if (formattedMessage.size() > 5000) {
        std::cerr << "Message size exceeds 5000 bytes" << std::endl;
        logMessage("Message size exceeds 5000 bytes");
        return;
    }

    // Send the message
    send(sock, formattedMessage.c_str(), formattedMessage.size(), 0);
}

// Helper function for Creating the "SERVERS" command back to server
std::string createServersResponse() {
    std::string response = "SERVERS";

    // Adding to SERVERS COMMAND
    response += "," + serverGroupId + "," + serverIpAddress+ "," + std::to_string(serverPort) + ";";

    // Add connected servers' information
    for (const auto &server : connectedServers) {
        response += server.group_id + "," + server.ip_address + "," + std::to_string(server.port) + ";";
    }
    return response;
}

// Send the SERVERS response with all connected servers to the server
void sendServersResponse(int serverSocket) {
    // Build the SERVERS response
    std::string response = createServersResponse();

    // Send the response
    sendMessage(serverSocket, response);
}

// Process command from Server to server
void serverCommand(int serverSocket, fd_set *openSockets, int *maxfds, char *buffer){
    (void)openSockets;
    (void)maxfds;
    
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

    // prints recieved command
    std::cout << "Recieved command: " << tokens[0] << std::endl;
    logMessage("Received command from server: " + tokens[0]);
    // prints whole recieved command
    std::cout << "Full command: " << buffer << std::endl << std::endl;
    logMessage("Full message from server " + message);

    //Hello command processing
    if (tokens[0] == "HELO" && tokens.size() >= 2) {
        std::string fromGroupId = tokens[1];

        struct sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);
        if (getpeername(serverSocket, (struct sockaddr*)&peer_addr, &peer_addr_len) == 0) {
            char host[NI_MAXHOST];
            if (getnameinfo((struct sockaddr *)&peer_addr, sizeof(peer_addr), 
                            host, sizeof(host), NULL, 0, NI_NUMERICHOST) == 0) {
                std::string fromIpAddress = host;
                int fromPort = ntohs(peer_addr.sin_port);

                // Store the connecting server's information
                connectedServers.push_back(Server(fromIpAddress, fromPort, fromGroupId));
                std::cout << "Added server " << fromGroupId << " at " << fromIpAddress << ":" << fromPort << std::endl;
                logMessage("Added server " + fromGroupId + " at " + fromIpAddress + ":" + std::to_string(fromPort));

                // Optionally send a response back to the connected server
                logMessage("Sending SERVERS command to server");
                sendServersResponse(serverSocket);
            }
        }

    } else if (tokens[0] == "SERVERS") {
        printf("**Servers command acknowledged**\n");
        logMessage("Servers command acknowledged");
        // Send the SERVERS response
        // Collect information from the SERVERS command about connected servers
        // Add servers from the SERVERS command to the connectedServers list
 
        for (size_t i = 1; i < tokens.size(); i += 3) {
            if (i + 2 < tokens.size()) { // Ensure there are enough tokens
                std::string group_id = tokens[i];
                std::string ip_address = tokens[i + 1];
                int port = 0;
                try {
                    port = std::stoi(tokens[i + 2]);
                } catch (const std::invalid_argument& e) {
                    std::cerr << "Invalid port number: " << tokens[i + 2] << std::endl;
                    logMessage("Invalid port number: " + tokens[i + 2]);
                    continue; // Skip this iteration
                }
                
                // Add the server to the connectedServers list
                connectedServers.push_back(Server(ip_address, port, group_id));
            }
        }
    } else if (tokens[0] == "GETMSGS") {
        printf("Received GETMSGS command from server\n");
        // Other server is requesting a messages
        // format of the message: MSG,group_id
        // format of message to send to another server: SENDMSG,<TO GROUP ID>,<FROM GROUP ID>,<Message content>

        if (tokens.size() >= 2) {
            std::string groupId = tokens[1];

            // Check if there are messages in the group message queue
            if (groupMessages.find(groupId) != groupMessages.end() && !groupMessages[groupId].empty()) {
                // Send the first message on the queue
                std::string message = groupMessages[groupId].front();
                groupMessages[groupId].pop();

                std::string fromGroupId = serverGroupId; // Server's group ID

                // Send the message to the server
                std::string response = "SENDMSG," + groupId + "," + fromGroupId + "," + message;

                //printing for debugging
                std::cout << "Sending message to server: " << response << std::endl;

                //using the sendMessage function to format and send the message
                sendMessage(serverSocket, response);
            } else {
                // No messages on the queue
                std::string noMessage = "NO_MSG," + groupId;
                send(serverSocket, noMessage.c_str(), noMessage.size(), 0);
            }
        } else {
            std::string errorMsg = "ERROR,GETMSGS: GETMSGS requires GroupID";
            send(serverSocket, errorMsg.c_str(), errorMsg.size(), 0);
        }
    } else if (tokens[0] == "SENDMSG") {
        printf("Received SENDMSG command from server\n");
        // Check if the message is to my group, if so, send it to the client
        // format of message to send to another server: SENDMSG,<TO GROUP ID>,<FROM GROUP ID>,<Message content>

        if (tokens.size() >= 4) {
            std::string toGroupId = tokens[1];
            std::string fromGroupId = tokens[2];
            std::string message = tokens[3];

            // Check if the message is for my group
            if (toGroupId == serverGroupId) {
                
                // Store the message in the group message marked for the client
                groupMessages[fromGroupId].push(message);
                
            } else {
                // message is not for my group, send it to the next server
                // Send the message to the next server
                // format of message to send to another server: SENDMSG,<TO GROUP ID>,<FROM GROUP ID>,<Message content>
                std::string response = "SENDMSG," + toGroupId + "," + fromGroupId + "," + message;
                
                
                std::map<std::string, int> serverSockets; // Map group_id to socket
                //
                if (serverSockets.find(toGroupId) != serverSockets.end()) {
                    int targetSocket = serverSockets[toGroupId];
                    sendMessage(targetSocket, response);
                } else {
                    // can not forward message to the next server since it is not connected
                    // store the message in the group message queue
                    printf("Cannot forward message, Storing message in the group message queue\n");
                    groupMessages[toGroupId].push(message);
                    
                }
                sendMessage(serverSocket, response);
            }
        } else {
            std::string errorMsg = "ERROR,SENDMSG: SENDMSG requires ToGroupID, FromGroupID, and Message";
            send(serverSocket, errorMsg.c_str(), errorMsg.size(), 0);
        }
        
    } else if (tokens[0] == "STATUSREQ" && tokens.size() >= 1) {
        printf("Received STATUSREQ command from server\n");
        // send the command STATUSRESP to followed by  "," and all messages in the group message queue
        // format of message to send to another server: STATUSRESP,<group_id of message 1>,<message1>,<group_id of message 2>,<message2>...
        std::string response = "STATUSRESP";
        for (auto &group : groupMessages) {
            std::string groupId = group.first;
            while (!group.second.empty()) {
                std::string message = group.second.front();
                group.second.pop();
                response += "," + groupId + "," + message;
            }
        }
        sendMessage(serverSocket, response);
        
    } else if (tokens[0] == "STATUSRESP") {
        printf("Received STATUSRESP command from server\n");

        
    }else {
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
    printf("Handling command from trusted client\n");
    std::cout << "Received command: " << tokens[0] << std::endl;
    std::cout << "Full command: " << buffer << std::endl;

    if (tokens[0].compare("LISTSERVERS") == 0)
    {
        // Construct the SERVERS response and send it back to client
        std::string response = "Response from server: LISTSERVERS";
        for (const auto &server : connectedServers)
        {
            response += "," + server.group_id + "," + server.ip_address + "," + std::to_string(serverPort) + ";";
        }
        send(clientSocket, response.c_str(), response.size(), 0);
    }
    // Client wants to send a message to a group
    else if (tokens[0].compare("SENDMSG") == 0) {
        if(tokens.size() >= 3) {

            // modifies the command: SENDMSG,<TO GROUP ID>,<FROM GROUP ID>,<Message content>
            std::string toGroupId = tokens[1];
            std::string fromGroupId = serverGroupId;
            std::string message = tokens[2];

            // format of message to send to another server: SENDMSG,<TO GROUP ID>,<FROM GROUP ID>,<Message content>
            std::string response = "SENDMSG," + toGroupId + "," + fromGroupId + "," + message;
            
            printf("Sending message to server:\n");

            // Send the message to the SENDMSG server command for processing
            char *mutableBuffer = strdup(response.c_str());
            serverCommand(clientSocket, openSockets, maxfds, mutableBuffer);
            free(mutableBuffer);


        }
    }
    else if (tokens[0].compare("GETMSG") == 0) {
        //looks for messages in the group message queue 
        if(tokens.size() >= 2) {

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

// Connect to a server with the given IP address and port
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
        logMessage("Connection to remote server failed");
        return -1;
    }

    return sock;
}

// Send a formatted HELO message to the server
void sendHeloMessage(int sock) {
    std::string heloCommand = "HELO," + serverGroupId; // Construct HELO message with the server's group ID
    sendMessage(sock, heloCommand); // Send the HELO message using the existing sendMessage function
    std::cout << "Sent: " << heloCommand << std::endl;
}

// Send a KEEPALIVE message to the server
void sendKeepAlive(int sock, int newMessages) {
    std::string keepAliveCommand = "KEEPALIVE," + std::to_string(newMessages);
    sendMessage(sock, keepAliveCommand);
    std::cout << "Sent: " << keepAliveCommand << std::endl;
}

// Periodically send KEEPALIVE messages to the server
void periodicKeepAlive(int sock) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::minutes(1)); // Sleep for one minute

        // Lock to safely access shared data
        std::lock_guard<std::mutex> guard(serverMutex);
        int newMessages = messagesWaiting[sock]; // Get the number of new messages waiting

        sendKeepAlive(sock, newMessages); // Send the KEEPALIVE message
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
    std::string serverIP;
    int remoteserverPort;

    initializeLogFile();
    logMessage("Server started");
    // Example usage to log a message
    if (argc != 2)
    {
        printf("Usage: chat_server <ip port>\n");
        exit(0);
    }
    

    serverGroupId = "A5_17";
    serverIpAddress = "130.208.246.249";
    serverPort = atoi(argv[1]);
    std::cout << "Server IP Address: " << serverIpAddress << std::endl;
    logMessage("Server IP Address: " + serverIpAddress);

    // Setup socket for server to listen to

    listenSock = open_socket(serverPort);
    printf("Listening on port: %d\n", serverPort);
    logMessage("Listening on port: " + std::to_string(serverPort));

    if (listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        logMessage("Listen failed on port: " + std::to_string(serverPort));
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


    // lets ask the user for the server ip address and port and then put it in the remoteserversock
    std::cout << "Enter the IP address of the server to connect to: ";
    std::cin >> serverIP;
    logMessage("Server IP Address: " + serverIP);
    
    std::cout << "Enter that servers port: ";
    std::cin >> remoteserverPort;
    logMessage("Server Port: " + std::to_string(remoteserverPort));

    // Connect to remote server 
    int remoteServerSock = connectToServer(serverIP, remoteserverPort);
    std::cout << "remoteserversock:  " << std::to_string(remoteServerSock) << std::endl;
    if (remoteServerSock != -1) {
        FD_SET(remoteServerSock, &openSockets);
        maxfds = std::max(maxfds, remoteServerSock);
        // connected to remote server ip: 
        std:: cout << "Connected to remote server" << std::endl;
        logMessage("Connected to remote server");
        sendHeloMessage(remoteServerSock);
        logMessage("Sent HELO message to remote server");
    }

    // while connected keep checking server command
    serverCommand(remoteServerSock, &openSockets, &maxfds, buffer);
    logMessage("Server command received from remote server");
    // print connectedservers list
    std::thread keepAliveThread(periodicKeepAlive, remoteServerSock);
    keepAliveThread.detach();

    // Create a new client object for the remote server
    Client *remoteServerClinet = new Client(remoteServerSock, serverIP, remoteserverPort);
    //add client1 to clinet map
    clients[remoteServerSock] = remoteServerClinet;

    //print connected servers list
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
            logMessage("Select failed - closing down");
            finished = true;
        }
        else
        {
            // First, accept  any new connections to the server on the listening socket
            if (FD_ISSET(listenSock, &readSockets))
            {
                clientLen = sizeof(client);
                clientSock = accept(listenSock, (struct sockaddr *)&client, &clientLen);
                printf("New Connection\n");
                logMessage("New Connection");
                printf("accept***\n");
                logMessage("accept***");
                printf("Client connected on server: %d\n", clientSock);
                logMessage("Client connected on server: " + std::to_string(clientSock));
                // Add new client to the list of open sockets
                FD_SET(clientSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = std::max(maxfds, clientSock);
                logMessage("Maxfds: " + std::to_string(maxfds));

                // Get client IP address and port
                std::string clientIp = inet_ntoa(client.sin_addr);
                logMessage("Client IP Address: " + clientIp);
                int clientPort = ntohs(client.sin_port);
                // SEND HELO HERE
                sendHeloMessage(clientSock);
                logMessage("Sent HELO message to client");
            
                // create a new client to store information.
                clients[clientSock] = new Client(clientSock, clientIp, clientPort);

                // Decrement the number of sockets waiting to be dealt with
                n--;

                
                
            }
            // Now check for commands from clients/Servers
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
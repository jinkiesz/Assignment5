# Assignment5
# README describes how to compile and run the code, lists the commands that are implemented in the client and server
# and describe the behaviour of the server in response to these commands and potentially other external events.


Welcome to Assignment5 done by group A5_17
    Group A5_17: Kristinn99? and Larak22@ru.is

Note!: All of the files must be placed in the TSAM Server 
Compile: to compile both of these files write "make" in the terminal, it will compile both of the files
Files: There are two executable files. client.cpp and tsamgroup17.cpp and one Makefile


Server Information, file(tsamgroup17.cpp):

    Running the server:
        Step1. to run the server first you need to compile the file by writing make in the terminal
        Step2. to execute the server write the name of the file and then the port you wish to open a socket on: ./tsamgroup17 <port> 
        Step3. Once youve run the Server a server_log.txt file should had been created thats gonna log the information
        info. after that you should see confirmation on what ip address your server is on and what port youve opened on a socket
        Step4. input ip of the server you want to remotely connect to, once youve input that you should get asked for the port number of that server. 
        Step5. congrats youve (hopefully) been connected and should had sent and recieved a HELO and SERVERS command 

    Commands Implemented in Server:
        Send HELO: sends HELO to anyone that connects to the server or anyone the server connects to
        Recieve HELO: once HELO has been recieved from a server, the server adds the group name, ip, port of that server into its servers list
        Recieve SERVERS: Once we recieve SERVERS command we add those given servers into our list
        KEEPALIVE: not really a command just a prompt that keeps alive connection every 1 minute
        GETMSGS: ?
        SENDMSG: ?

    Note!: The server does not run locally and has to be placed in the TSAM server 

Client Information, file(client.cpp):
    ? dno whats been done ?


Extra Points to claim:
(a) (1 point) Submit your solution for the client/server protocol implementation within the first week.
(e) (2 points) Or: Write a brief - no more than 1 page - description listing the several security issues of the botnet.

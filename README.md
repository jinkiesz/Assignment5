# Assignment5
# README describes how to compile and run the code, lists the commands that are implemented in the client and server
# and describe the behaviour of the server in response to these commands and potentially other external events.


Welcome to Assignment5 done by group A5_17
    Group A5_17: Kristinn99? and Larak22@ru.is

Compile: to compile both of these files write "make" in the terminal, it will compile both of the files
There are two executable files. client.cpp and tsamgroup17.cpp


Server Information, file(tsamgroup17.cpp):

    Running the server:
        Step1. to run the server first you need to compile the file by writing make in the terminal
        Step2. to execute the server write the name of the file and then the port you wish to open a socket on: ./tsamgroup17 <port> 
        info. after that you should see confirmation on what ip address your server is on and what port youve opened on a socket
        Step3. input ip of the server you want to remotely connect to, once youve input that you should get asked for the port number of that server. 
        Step4. congrats youve (hopefully) been connected and should had sent and recieved a HELO and SERVERS command 

    Commands Implemented in Server:
        Send HELO: sends HELO to anyone that connects to the server or anyone the server connects to
        Recieve HELO: once HELO has been recieved from a server, the server adds the group name, ip, port of that server into its servers list
        Recieve SERVERS: Once we recieve SERVERS command we add those given servers into our list
        KEEPALIVE: not really a command just a prompt that keeps alive connection every 1 minute
        GETMSGS: ?
        SENDMSG: ?


Client Information, file(client.cpp):
    ? dno whats been done ?
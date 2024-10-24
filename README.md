# Assignment5
# README describes how to compile and run the code, lists the commands that are implemented in the client and server
# and describe the behaviour of the server in response to these commands and potentially other external events.


Welcome to Assignment5 done by group A5_17
    Group A5_17: Kristinng20@ru.is and Larak22@ru.is

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
        Send HELO: 
            sends HELO to anyone that connects to the server or anyone the server connects to
        Recieve HELO: 
            once HELO has been recieved from a server, the server adds the group name, ip, port of that server into its servers list
        Recieve SERVERS: 
            Once we recieve SERVERS command we add those given servers into our list
        KEEPALIVE: 
            not really a command just a prompt that keeps alive connection every 1 minute
        SENDMSG: 
            If the message is to my group -> stores it in my message queue. else:
            tries to deliver a formatted SENDMSG command to the server if he is connected, else store it in the message queue.
        GETMSGS:
            Looks for all messages for that group and and formats it to SENDMESSAGE command dilivered to that server if connected else stored in message queue

    Note!: The server does not run locally and has to be placed in the TSAM server 

Client Information, file(client.cpp):
    
    Running the Client:
        step1. Compile the file by writing "make" in the terminal
        step2. excecute client: ./client_chat <port>
        step3. The client tries to connect to server on start up
        step3. run client commands (see in next step)

    Commands Implemented in Client:
        i: All commands sent from my client are prompted normally but send "secret_" in front of all prompt to my server so that he knows that the command is comming from a "trusted client/my client"

        GETMSG,<groupID>: 
            The server looks for a message on the message queue for the groupID and sends it back to client.

        SENDMSG,<GROUP ID>,<message contents>:
            The Server tries to send this message to a connected server, else it stores it on the groupMessage queue. 
            <my group ID> it stores it dirrectly on the message queue.

        GETMSG,<GROUP ID>:
            The server looks for a message on the message queue with the group id and sends it back to client.

        LISTSERVERS:
            Sends formatted lists of all connected servers

        Example:
            Sending SENDMSG,A5_17,messega the server will store this message on the message queue.
            When Client then prompts GETMSG,A5_17 the server fetches the message from the queue and delivers back to client.





Extra Points to claim:
(a) (1 point) Submit your solution for the client/server protocol implementation within the first week.
(e) (2 points) Or: Write a brief - no more than 1 page - description listing the several security issues of the botnet.

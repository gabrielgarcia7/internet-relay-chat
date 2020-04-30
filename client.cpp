#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <thread>
#include <atomic>

#define BUFFER_SIZE 4096 // max char amount of buffer 4096 + largest nickname length
#define NICK_SIZE 50

char nickname[NICK_SIZE];
char buffer[BUFFER_SIZE];
char message[BUFFER_SIZE+NICK_SIZE+2];
int socketClient, portNum, n;
std::atomic<bool> flag (false);

/*
    Function that prints the error and returns 1.

*/
void error(const char *msg){
    perror(msg);
    exit(1);
}

/*
    Function that verifies the commands typed by the user.

*/
void userCommand(char message[]){
    if (strcasecmp(message, "\\quit") == 0){    // if user wants to leave chat
        flag = true;
    }
}

/*
    Function that reads the client's nickname.

*/
void read_nickname(){

    char buffer_nick[256];

    printf("-> Type your nickname (maximum of fifty characters): ");
    fgets(buffer_nick, 256, stdin);

    if(strlen(buffer_nick) > 50){
        printf("-> The nickname exceeds 50 characters. The first 50 characters will be considered.\n");
        buffer_nick[NICK_SIZE] = '\0';
    }
    else{
        buffer_nick[strlen(buffer_nick)-1] = '\0';
    }
    
    strcpy(nickname, buffer_nick);
    buffer_nick[0] = '\0';
}

/*
    

*/
void sendController(){

    while(!flag){
        bzero(buffer,BUFFER_SIZE); // clears buffer
        fgets(buffer, BUFFER_SIZE, stdin); // reads message from input
        buffer[strlen(buffer)-1] = '\0';
            
        if(buffer[0] == '\\'){
            userCommand(buffer);
        }
        else {
            sprintf(message, "%s: %s", nickname, buffer);
            n = write(socketClient, message, strlen(message));    // sends message to server
            if (n == -1) 
            error("!!! Error writing to socket ");
        }
    }
}

/*
    

*/
void receiveController(){
    while(!flag){
        bzero(buffer,BUFFER_SIZE); // clears buffer
        n = read(socketClient, buffer, BUFFER_SIZE); // receives message from server
        if (n == -1) 
            error("!!! Error reading from socket !!!");
        else if (n == 0 || strcasecmp("quit\n", buffer) == 0) {
            printf("Server has left");

            flag = true;
        }
        else if(n > 1) printf("%s\n",buffer);
    }
}

int main(int argc, char *argv[]){
    
    struct sockaddr_in serverAddress;
    struct hostent *server;    

    /* 
    ARGUMENTS:
        - argv[0] == client proccess name;
        - argv[1] == server host machine name;
        - argv[2] == port number.
    */
    if (argc < 3) {
       printf("!!! Error, bad proccess call, try: %s hostname port !!!\n", argv[0]);
       exit(1);
    }

    // -- CONNECTING TO THE SERVER --
    portNum = atoi(argv[2]); // sets port number;
    socketClient = socket(AF_INET, SOCK_STREAM, 0); // creates a socket;
    if (socketClient == -1)
        error("!!! Error while opening socket !!!\n");

    server = gethostbyname(argv[1]); // return entry from host data base for host with NAME.

    if (server == NULL)
        error("!!! Error, host not found !!!\n");
    
    bzero( (char *) &serverAddress, sizeof(serverAddress)); // initializes serverAddress with 0s    

    serverAddress.sin_family = AF_INET; // setting serverAddress values
    bcopy( (char *)server->h_addr, (char *)&serverAddress.sin_addr.s_addr, server->h_length);   // copies server IP address into serverAddress
    serverAddress.sin_port = htons(portNum);    // htons() method converts int into network byte order

    if (connect(socketClient, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1)
        error("!!! Error connecting !!!\n");

    n = read(socketClient, buffer, 4095); // receives message from server
    if (n == -1)    // message should confirm connection success
         error("!!! Error reading from socket !!!");

    if(strcasecmp(buffer, "Chat full") == 0){
        printf("---- Sorry, but the chat is full. Connection refused. ;( ----\n");
        close(socketClient);
        return 0;
    }
    printf("\n%s", buffer);

    printf("\n --------------------------------------");
    printf("\n|                                      |");
    printf("\n|         Welcome to the chat!         |");
    printf("\n|                                      |");
    printf("\n --------------------------------------\n\n");

    // reads nickname and sends it to the server;
    read_nickname();
    write(socketClient, nickname, NICK_SIZE);    

    printf("\n-> Hi, %s. The chat is ready for conversation!\n-> Type \"\\quit\" at any time to leave the chat.\n\n", nickname);

    // if the code gets here without any error, it is connected and ready to send and receive messages; 

    //Creating two threads, one to send messages and one to receive messages;
    std::thread sendMessages (sendController);
    sendMessages.detach();
    std::thread receiveMessages (receiveController);
    receiveMessages.detach();

    // while not receiving the signal to disconnect, continues...
    while(!flag){}

    printf("\n--- Leaving the server... Goodbye! ---\n\n");

    // Close the socket and return 0;
    close(socketClient);
    return 0;
}
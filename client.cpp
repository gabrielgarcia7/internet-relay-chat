 /*
    Computer Network SSC-0142

    ---- Internet Relay Chat ----
    Module 2 - Communication between multiple clients and server

    Caio Augusto Duarte Basso NUSP 10801173
    Gabriel Garcia Lorencetti NUSP 10691891

    Client

 */

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
#include <signal.h> 

#define BUFFER_SIZE 2048 // max char amount of buffer
#define REC_BUFFER_SIZE 4096 // 2x max char amount of buffer (used if the message exceeds BUFFER_SIZE)
#define NICK_SIZE 50 // max char amount of nickname
#define BUFFER_SIZE_MAX 40960 // max char amount of bufferMax

// Function headers
void userCommand(char*);
void readNickname();
void sendController();
void receiveController();
void connectServer(char*, int);
void printCommands();
void connectChannel(char*);

struct sockaddr_in serverAddress; // server adress
struct hostent *server;  // informations about the server
char nickname[NICK_SIZE]; // nickname of the client
char buffer[BUFFER_SIZE]; // message to write to client
char recBuffer[REC_BUFFER_SIZE]; // message to write to client (used if the message exceeds BUFFER_SIZE)
char bufferMax[BUFFER_SIZE_MAX]; // max message (used if the message exceeds BUFFER_SIZE)
char message[BUFFER_SIZE+NICK_SIZE+2]; // message to write to client + nickname
int socketClient, portNum, n; // informations of the client
std::atomic<bool> flag (false); // flag used to stop the application
std::atomic<bool> connected (false); // flag used to stop the application

/*
    Function that prints the error and returns 1.
*/
void error(const char *msg){
    perror(msg);
    exit(1);
}

/*
    Function that sends a message to the server.
*/
void sendMessage(char bufferMax[]){

    // In case of the message exceeds the BUFFER_SIZE, splits in several messages;
    if(strlen(bufferMax) > BUFFER_SIZE){
        int tam = strlen(bufferMax);
        int aux = 0;

        while(tam > BUFFER_SIZE){
            strncpy(buffer, bufferMax+aux, BUFFER_SIZE);
            buffer[BUFFER_SIZE] = '\0';

            if (aux == 0)
                sprintf(message, "%s", buffer);
            else sprintf(message, "\n%s", buffer);
            
            n = write(socketClient, message, strlen(message));    // sends message to server
            if (n == -1) 
                error("!!! Error writing to socket ");

            bzero(buffer, BUFFER_SIZE); // clears buffer
            bzero(message,BUFFER_SIZE+NICK_SIZE+2); // clears message

            tam -= BUFFER_SIZE;
            aux += BUFFER_SIZE;
        }
        strncpy(buffer, bufferMax+aux, tam);
    }
    else{
        strcpy(buffer, bufferMax);
    }

    sprintf(message, "%s", buffer);
    n = write(socketClient, message, strlen(message));    // sends message to server
    if (n == -1) 
        error("!!! Error writing to socket ");
}

/*
    Function that verifies the commands typed by the user.
*/
void userCommand(char command[]){
    if (strcasecmp(command, "/quit") == 0){    // if user wants to leave chat
        printf("%s", command);
        flag = true;
        connected = true;
    }

    else if (strcasecmp(command, "/c") == 0){ // if user wants to connect to server
        // char hostname[256];
        // char portnumber[256];

        // printf("\n-> Type the hostname and the port number to connect to the server.\n\n->The default is:\n"
        //             "  - Hostname: localhost\n  - Port number: 52547\n\n->If you want to cancel connection,"
        //             "type ABORT\n(Keep in mind that if you already connected to another chat, it will disconnect"
        //             "from current room!)\n");
        
        // printf("\nHostname: ");
        // fgets(hostname, 256, stdin);
        // hostname[strlen(hostname)-1] = '\0';

        // if(strcasecmp(hostname, "ABORT") == 0)
        //     return;
        
        // printf("Port number: ");
        // fgets(portnumber, 256, stdin);
        // portnumber[strlen(portnumber)-1] = '\0';

        // if(strcasecmp(portnumber, "ABORT") == 0)
        //     return;
        char hostname[256] = "localhost";
        char portnumber[256] = "52547";
        connectServer(hostname, atoi(portnumber));
    }

    else if (strcasecmp(command, "/ping") == 0){    // if user wants to check latency to server
        if(connected) sendMessage(command);
        else printf("\n->You are not connected to any chat yet!\n"
                        "-> Use the /connect command first to connect to a server.\n\n");
    }

    else if (strcasecmp(command, "/help") == 0){    // if user asks for commands
        printCommands();
        printf("\n");
    }

    else if (strncasecmp(command, "/join ", 6) == 0){    // if user wants to join a channel
        sendMessage(command);
    }

    else if (strncasecmp(command, "/nickname ", 10) == 0){    // if user wants to change nickname
        sendMessage(command);
    }
    
    else if ((strncasecmp(command, "/kick ", 6) == 0) || (strcasecmp(command, "/mute") == 0) || (strcasecmp(command, "/unmute") == 0) || (strcasecmp(command, "/whois") == 0)){  // admin commands
        if(connected) sendMessage(command);
        else printf("\n->You are not connected to any chat yet!\n"
                        "-> Use the /connect command first to connect to a server.\n\n");
    }

}

/*
    Function that reads the client's nickname.
*/
void readNickname(){

    char buffer_nick[256];

    printf("-> Type your nickname (maximum of fifty characters): ");
    fgets(buffer_nick, 256, stdin);

    if(strlen(buffer_nick) > 50){
        printf("-> The nickname exceeds 50 characters. The first 50 characters"
                "will be considered.\n");
        buffer_nick[NICK_SIZE] = '\0';
    }
    else{
        buffer_nick[strlen(buffer_nick)-1] = '\0';
    }
    
    strcpy(nickname, buffer_nick);
    buffer_nick[0] = '\0';
}

/*
    Function responsible for sending messages to the server.
*/
void sendController(){

    while(!flag){
        bzero(bufferMax,BUFFER_SIZE_MAX); // clears bufferMax
        bzero(message,BUFFER_SIZE+NICK_SIZE+2); // clears message
        fgets(bufferMax, BUFFER_SIZE_MAX, stdin); // reads message from input
        bufferMax[strlen(bufferMax)-1] = '\0';
        
        if (feof(stdin)) strcpy(bufferMax, "/quit");
        if(bufferMax[0] == '/'){ // if it's a command, check 
            userCommand(bufferMax);
        }

        else {
            sendMessage(bufferMax);
        }
    }
}

/*
    Function responsible for receiving messages from the server.
*/
void receiveController(){
    while(!flag){
        bzero(recBuffer,REC_BUFFER_SIZE); // clears recBuffer
        n = read(socketClient, recBuffer, REC_BUFFER_SIZE); // receives message from server
        if (n == -1) 
            error("!!! Error reading from socket !!!");
        else if (/*n == 0 || */strcasecmp("quit\n", recBuffer) == 0) { // if the server is down
            printf("recbuffer: %s\n\nn: %d\n", recBuffer, n);
            printf("Server has left");
            //flag = true;
        }
        else if(strcasecmp("kicked", recBuffer) == 0){
            printf("triste\n\n");
            flag = true;
        }
        else if(n > 1) printf("%s\n",recBuffer); // prints message
    }
}

/*
    Function responsible for connecting to the server.
*/
void connectServer(char *serverName, int serverPort){

    portNum = serverPort; // sets port number;
    socketClient = socket(AF_INET, SOCK_STREAM, 0); // creates a socket;
    if (socketClient == -1)
        error("!!! Error while opening socket !!!\n");

    server = gethostbyname(serverName); // return entry from host data base for host with NAME.

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

    // If the chat is already full, exits
    if(strcasecmp(buffer, "Chat full") == 0){
        printf("---- Sorry, but the chat is full. Connection refused. ;( ----\n");
        close(socketClient);
        exit(1);
    }
    connected = true;

    printf("\n%s", buffer);

    // reads nickname and sends it to the server;
    readNickname();
    write(socketClient, nickname, NICK_SIZE);    

    printf("\n\n-> Hi, %s. Welcome to our chat!\n\n\n", nickname);

    printf("-> You can type \"/quit\" at any time to leave the chat and \"/help\" to see the other commands.\n\n");

    printf("-> Use the command \"/join channelName\" to enter a channel and start chatting.\n");
    printf("(Remembers that channel names must be less than 200 characters and start with '&' or '#'. Also, it must not contain spaces, CTRL + G or commas.)\n\n\n");


    // if the code gets here without any error, it is connected and ready to send and receive messages; 

    //Creating two threads, one to send messages and one to receive messages;
    std::thread sendMessages (sendController);
    sendMessages.detach();
    std::thread receiveMessages (receiveController);
    receiveMessages.detach();
}

/*
    Function for making the first connection.
*/
void first_connection(){
    bzero(bufferMax,BUFFER_SIZE); // clears bufer
    fgets(bufferMax, BUFFER_SIZE_MAX, stdin); // reads message from input
    bufferMax[strlen(bufferMax)-1] = '\0';
    
    
    if (feof(stdin)) strcpy(bufferMax, "/quit");
    if(bufferMax[0] == '/'){ // if it's a command, check 
        userCommand(bufferMax);
    }
    
}

/*
    Function for printing commands.
*/
void printCommands(){
    printf("\n-> Commands:\n\n");
    printf("   - /connect (establishes the connection to the server given a port)\n");
    printf("   - /quit (ends the connection and closes the application)\n");
    printf("   - /ping (the server returns pong as soon as it receives the message)\n");
}

void printChannelCommands(){
    printf("\n-> Channel commands :\n\n");
    printf("   - /join channelName (enters a channel)\n");
    printf("   - /nickname desiredNickname (the client is recognized by the specified nickname)\n");
    printf("   - /ping (the server returns pong as soon as it receives the message)\n");
}

void printAdminCommands(){
    printf("\n-> Administrator commands :\n\n");
    printf("   - /kick username (closes the connection for a specified user)\n");
    printf("   - /mute username (prevents a user from being able to send messages on this channel)\n");
    printf("   - /unmute username (remove the user's mute)\n");
    printf("   - /whois username (returns the user's IP address only to the administrator)\n");
}


/*
    Function to print welcome message.
*/
void print_welcome_message(){
    printf("\n --------------------------------------");
    printf("\n|                                      |");
    printf("\n|         Welcome to the chat!         |");
    printf("\n|                                      |");
    printf("\n --------------------------------------\n");

    printCommands();
    printf("   - /help (shows available commands)\n\n");

}

/*
    Function to deal with Ctrl+C.
*/
void sigintHandler(int sig_num){
    signal(SIGINT, sigintHandler); 
    printf("\n---- Cannot be terminated using Ctrl+C ----\n"); 
    printf("-> Instead, use the /quit command or Ctrl+D.\n");
    fflush(stdout); 
} 

int main(int argc, char *argv[]){
    
    signal(SIGINT, sigintHandler);

    print_welcome_message();

    // while not connect to the server...
    while(!connected) first_connection();

    // while not receiving the signal to disconnect, continues...
    while(!flag){}

    printf("\n--- Leaving the application... Goodbye! ---\n\n");

    // Close the socket and return 0;
    close(socketClient);
    return 0;
}
 /*
    Computer Network SSC-0142

    ---- Internet Relay Chat ----
    Module 2 - Communication between multiple clients and server

    Caio Augusto Duarte Basso NUSP 10801173
    Gabriel Garcia Lorencetti NUSP 10691891

    Server

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <thread>
#include <pthread.h>
#include <mutex>
#include <atomic>
#include <signal.h> 

#define MAXCLIENTS 10   // max number of clients connected to server
#define BUFFER_SIZE 2048 // max char amount of buffer
#define NICK_SIZE 50 // max char amount of nickname
#define BUFFER_SIZE_MAX 40960 // max char amount of bufferMax

int clientsNum = 0; // number of clients connected
char buffer[BUFFER_SIZE];  // message to write to client
char bufferMax[BUFFER_SIZE_MAX]; // max message (used if the message exceeds BUFFER_SIZE)
char message[BUFFER_SIZE+NICK_SIZE+2]; // message to write to client + nickname
bool flagFull = true; // used to verify if the server is full
int serverSocket; // socket of the server
static bool flagMsg = false; // used to check the consistency of the nickname
std::mutex mtx; // used to lock and unlock threads to send messages
std::atomic<bool> flag (false); // flag used to stop the application

typedef struct client {
    int socketId;
    struct sockaddr_in address;
    char nickname[NICK_SIZE];
    int usrID;
    bool connected;
}CLIENT;

CLIENT clients[MAXCLIENTS]; // stores every client connected to server

/*
    Function that prints the error message and returns 1.
*/
void error(const char *msg){
    perror(msg);
    exit(1);
}

/*
    Function that verifies the commands typed by the user.
*/
void userCommand(char message[]){
    if (strcasecmp(message, "/quit") == 0){    // if server wants to leave chat
        flag = true;

        printf("\n--- Leaving the application... Goodbye! ---\n\n");

        for (int j = 0; j < clientsNum; j++) // closes all client sockets
            close(clients[j].socketId);
        close(serverSocket);
        exit(0);
    }
}

/*
    Function to deal with Ctrl+C.
*/
void sigintHandler(int sig_num){
    signal(SIGINT, sigintHandler); 
    printf("\n---- Cannot be terminated using Ctrl+C ----\n"); 
    printf("-> Instead, use the /quit command or Ctrl+D\n\n");
    fflush(stdout); 
} 

/*
    Function that sends a message to the clients.
    - If sendAll is true, send it to everyone.
    - If sendAll is false, send to all but the specified userId.
*/
void sendMessage(char* message, int userID, bool sendAll) {

    mtx.lock();

    for(int i = 0; i < MAXCLIENTS; i++){
        if((clients[i].usrID != userID && clients[i].connected == true) || (sendAll==true && clients[i].connected == true)){
            
            bool sended = false;
            int count = 0;

            while(sended == false && count < 5){
                if (write(clients[i].socketId, message, strlen(message)) < 0 && flagMsg == false) {                
                    count++;
                }
                else sended = true;
            }

            if(count >= 5 && sended == false){
                error("!!! Error sending a message !!!\n");
                close(clients[i].usrID);
            }
        }
    }

    mtx.unlock();
}

/*
    Function that controls the connected client.
    Receives the nickname and handles receiving messages and sending 
    them to other clients.
*/
void clientController(CLIENT client){
    
    char nick[NICK_SIZE];

    flagMsg = false;
    clientsNum++;

    if(recv(client.socketId, nick, NICK_SIZE, 0) <= 0){
        error("!!! Error receiving nickname !!!\n");
        flagMsg = true;
    }
    else{
        strcpy(client.nickname, nick);

        sprintf(message, "---- %s joined the chat! ----\n\n", client.nickname);
        printf("%s", message);
    }
    
    bzero(message, BUFFER_SIZE+NICK_SIZE+2); // clears message
    
    while(!flagMsg){ // while the client is connected;

        int receive = recv(client.socketId, message, BUFFER_SIZE+NICK_SIZE+2, 0);

        if(receive > 0){
            if (strlen(message) > 0) {
                
                if (strstr(message, "/ping")){  // if message is ping request
                    write(client.socketId, "pong", strlen("pong"));     // fazer uma funcao so pra isso?
                }
                else{
                    sendMessage(message, client.usrID, false);
                    
                    // Prints on the server who sent the message to whom
                    printf("%s\n", message); 
                }
            }
        }
        else if (receive == 0 || strcasecmp("quit\n", message) == 0) {
            sprintf(message, "\n---- %s has left. ----\n\n", client.nickname);
            printf("%s", message);

            sendMessage(message, client.usrID, false);

            flagMsg = true; // disconnect
        } else {
            error("!!! Error !!!\n");
            flagMsg = true; // disconnect
        }
        bzero(message, BUFFER_SIZE+NICK_SIZE+2); // clears message
    }

    // remove client
    client.connected = false;
    close(client.socketId);
    clientsNum--;
    flagFull = false;
}

/*
    Function responsible for handling server 
    messages for all clients.
*/
void sendController(){

    while(!flag){
        bzero(bufferMax,BUFFER_SIZE_MAX); // clears buffer
        bzero(message,BUFFER_SIZE+NICK_SIZE+2); // clears buffer
        fgets(bufferMax, BUFFER_SIZE_MAX, stdin); // reads message from input
        bufferMax[strlen(bufferMax)-1] = '\0';
        
        if (feof(stdin)) strcpy(bufferMax, "/quit");
        if(bufferMax[0] == '/'){
            userCommand(bufferMax);
        }
        else {

            // In case of the message exceeds the BUFFER_SIZE, splits in several messages;
            if(strlen(bufferMax) > BUFFER_SIZE){
                int tam = strlen(bufferMax);
                int aux = 0;

                while(tam > BUFFER_SIZE){
                    strncpy(buffer, bufferMax+aux, BUFFER_SIZE);
                    buffer[BUFFER_SIZE] = '\0';

                    if (aux == 0)
                        sprintf(message, "Server: %s", buffer);
                    else sprintf(message, "\nServer: %s", buffer);
                    
                    sendMessage(message, 0, true);
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
            sprintf(message, "Server: %s", buffer);
            sendMessage(message, 0, true); // send message to all clients
        }
    }
}


int main(int argc, char *argv[]){
    int clientSocket;  // file descriptors for starting socket and socket after client connected
    int portNum; // stores port number utilized
    socklen_t clientAdLength;   // stores size of client address
    struct sockaddr_in serverAddress, clientAddress;    // internet address, defined in netinet/in.h
    char welcomeMessage[60] = "\n---- Successfully connected to the server. ----\n\n";

    signal(SIGINT, sigintHandler);
    
    if (argc < 2) { // argv[0] == server proccess name, argv[1] == port number
        error("!!! Error, no port number provided. Provide port number while calling proccess !!!");
    }
    portNum = atoi(argv[1]);    // sets port number

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);  // creates a socket
    // AF_INET -- internet connection; SOCK_STREAM -- message in continuous stream; 0 -- OS chooses protocol

    if (serverSocket == -1) // socket() method returns -1 in case of error 
        error("!!! Error while opening socket !!!");

    bzero( (char *) &serverAddress, sizeof(serverAddress));  // initializes serverAddress with 0s
    serverAddress.sin_family = AF_INET; // set serverAddress values
    serverAddress.sin_addr.s_addr = INADDR_ANY; // IP address of machine running the server
    serverAddress.sin_port = htons(portNum);    // htons() method converts int into network byte order

    int enable = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        error("!!! setsockopt(SO_REUSEADDR) failed !!!");

    if (bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) // binds socket to port
        error("!!! Error while binding, chosen port possibly already being used !!!");

    listen(serverSocket, 5); // proccess listens on socket for connections
                        // 5 refers to max number of pending connections in queue

    printf("\n ---------------------------------------------");
    printf("\n|                                             |");
    printf("\n|         Welcome to the chat server!         |");
    printf("\n|                                             |");
    printf("\n ---------------------------------------------");
    printf("%s\n\n-> Type \"/quit\" or Ctrl+D to close the server\n\n",buffer);

    // creates a thread to send messages from the server to everyone
    std::thread sendMessagesToAll (sendController);
    sendMessagesToAll.detach();

    clientAdLength = sizeof(clientAddress);

    while(!flag){ // while the server is on

        clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &clientAdLength);

        if(MAXCLIENTS == (clientsNum)){ // if the chat is full
            if(flagFull){
                printf("\n---- A client tried to connect, but the chat is full. Connection refused. ;( ----\n\n");                
                write(clientSocket, "Chat full", strlen("Chat full"));
                close(clientSocket);
            }
        }

        else{        
            if(clientSocket >= 0){ // if a client tries to connect
                write(clientSocket, welcomeMessage, strlen(welcomeMessage));    // sends message to server

                clients[clientsNum].address = clientAddress;
                clients[clientsNum].socketId = clientSocket;
                clients[clientsNum].connected = true;
                clients[clientsNum].usrID = clientsNum;

                // creates a thread to the new client connected;
                std::thread client (clientController, clients[clientsNum]);
                client.detach();
            }
        }       

        if(flag) return 0;

        // to consume less cpu
        sleep(2);
    }

    printf("\n--- Leaving the application... Goodbye! ---\n\n");


    for (int j = 0; j < clientsNum; j++) // closes all client sockets
        close(clients[j].socketId);
    close(serverSocket); // close server socketsA

    return 0; 
}
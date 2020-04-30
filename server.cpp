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

#define MAXCLIENTS 2   // max number of clients connected to server
#define BUFFER_SIZE 4096 // max char amount of buffer 4096 + largest nickname length
#define NICK_SIZE 50

int clientsNum = 0;
char buffer[BUFFER_SIZE];  // message to write to client
std::atomic<bool> flag (false);
char message[BUFFER_SIZE+NICK_SIZE+2];
bool flagFull = true;
std::mutex mtx;
int serverSocket;
static bool flagMsg = false;

typedef struct client {
    int socketId;
    struct sockaddr_in address;
    char nickname[NICK_SIZE];
    int usrID;
    bool connected;
}CLIENT;

CLIENT clients[MAXCLIENTS]; // stores every client connected to server

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
    if (strcasecmp(message, "\\quit") == 0){    // if server wants to leave chat
        flag = true;
        for (int j = 0; j < clientsNum; j++) // closes all client sockets
            close(clients[j].socketId);
        close(serverSocket);
        exit(0);
    }
}

/*
    

*/
void send_message(char* message, int userID, bool sendAll) {

    mtx.lock();

    for(int i = 0; i < MAXCLIENTS; i++){
        if((clients[i].usrID != userID && clients[i].connected == true) || (sendAll==true && clients[i].connected == true))
            if (write(clients[i].socketId, message, strlen(message)) < 0 && flagMsg == false) {
                error("!!! Error sending a message !!!\n");
            }
    }

    mtx.unlock();
}

/*
    

*/
void clientController(CLIENT client){
    
    char buffer[BUFFER_SIZE];
    char nick[NICK_SIZE];

    flagMsg = false;
    clientsNum++;

    if(recv(client.socketId, nick, NICK_SIZE, 0) <= 0){
        error("!!! Error receiving nickname !!!\n");
        flagMsg = true;
    }
    else{
        strcpy(client.nickname, nick);

        sprintf(buffer, "---- %s joined the chat! ----\n\n", client.nickname);
        printf("%s", buffer);
    }
    
    bzero(buffer, BUFFER_SIZE);
    
    while(!flagMsg){

        int receive = recv(client.socketId, buffer, BUFFER_SIZE, 0);

        if(receive > 0){
            if (strlen(buffer) > 0) {
                send_message(buffer, client.usrID, false);
                
                // Prints on the server who sent the message to whom
                printf("%s\n", buffer); 
            }
        }
        else if (receive == 0 || strcasecmp("quit\n", buffer) == 0) {
            sprintf(buffer, "\n---- %s has left. ----\n\n", client.nickname);
            printf("%s", buffer);

            send_message(buffer, client.usrID, false);

            flagMsg = true;
        } else {
            error("!!! Error !!!\n");
            flagMsg = true;
        }
        bzero(buffer, BUFFER_SIZE);
    }

    client.connected = false;
    close(client.socketId);
    clientsNum--;
    flagFull = false;

}

/*
    

*/
void sendController(){

    while(!flag){
        bzero(buffer,BUFFER_SIZE); // clears buffer
        bzero(message,BUFFER_SIZE+NICK_SIZE+2); // clears buffer
        fgets(buffer, BUFFER_SIZE, stdin); // reads message from input
        buffer[strlen(buffer)-1] = '\0';
            
        if(buffer[0] == '\\'){
            userCommand(buffer);
        }
        else {
            sprintf(message, "Server: %s", buffer);
            send_message(message, 0, true);
        }
    }
}


int main(int argc, char *argv[]){
    int clientSocket;  // file descriptors for starting socket and socket after client connected
    int portNum; // stores port number utilized
    socklen_t clientAdLength;   // stores size of client address
    struct sockaddr_in serverAddress, clientAddress;    // internet address, defined in netinet/in.h
    char welcomeMessage[60] = "\n---- Successfully connected to the server. ----\n\n";
    
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
    printf("%s\n\n-> Type \"\\quit\" to close the server\n\n",buffer);

    std::thread sendMessagesToAll (sendController);
    sendMessagesToAll.detach();
    clientAdLength = sizeof(clientAddress);

    while(!flag){

        clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &clientAdLength);

        if(MAXCLIENTS == (clientsNum)){
            if(flagFull){
                printf("\n---- A client tried to connect, but the chat is full. Connection refused. ;( ----\n\n");                
                write(clientSocket, "Chat full", strlen("Chat full"));
                close(clientSocket);
            }
        }

        else{        
            if(clientSocket >= 0){
                write(clientSocket, welcomeMessage, strlen(welcomeMessage));    // sends message to server

                clients[clientsNum].address = clientAddress;
                clients[clientsNum].socketId = clientSocket;
                clients[clientsNum].connected = true;
                clients[clientsNum].usrID = clientsNum;

                std::thread client (clientController, clients[clientsNum]);
                client.detach();
            }
        }       

        if(flag) return 0;
        sleep(2);
    }


    for (int j = 0; j < clientsNum; j++) // closes all client sockets
        close(clients[j].socketId);
    close(serverSocket);

    return 0; 
}
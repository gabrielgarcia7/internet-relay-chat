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
#include <algorithm> 
#include <list>
#include <vector>

#define MAXCLIENTS 10   // max number of clients connected to server
#define BUFFER_SIZE 2048 // max char amount of buffer
#define NICK_SIZE 50 // max char amount of nickname
#define BUFFER_SIZE_MAX 40960 // max char amount of bufferMax

typedef struct CLIENT client;
typedef struct CHANNEL channel;
int clientsNum = 0; // number of clients connected
char buffer[BUFFER_SIZE];  // message to write to client
char bufferMax[BUFFER_SIZE_MAX]; // max message (used if the message exceeds BUFFER_SIZE)
char message[BUFFER_SIZE+NICK_SIZE+2]; // message to write to client + nickname
bool flagFull = true; // used to verify if the server is full
int serverSocket; // socket of the server
static bool flagMsg = false; // used to check the consistency of the nickname
std::mutex mtx; // used to lock and unlock threads to send messages
std::atomic<bool> flag (false); // flag used to stop the application

struct CLIENT {
    int socketId;
    struct sockaddr_in address;
    char nickname[NICK_SIZE];
    int usrID;
    bool connected;
    char channelName[NICK_SIZE];
    bool muted;
    bool isAdmin = false;
};

struct CHANNEL {
    char name[NICK_SIZE];
    std::list <CLIENT> connected;   // stores clients
    CLIENT admin;
    std::list <CLIENT> mutedUsers;   // stores muted
    bool active;
};

std::vector<CLIENT> clients; // stores every client connected to server
std::vector<CHANNEL> channels;   // stores every channel
int channelAmnt = 0;

/*
    Function that adds a channel to the server
    Channel is created with an admin already connected
*/
void addChannel(char* name, CLIENT *admin){
    strcpy(admin->channelName, name);
    printf("str cpy do client criacao: %s\n", admin->channelName);
    admin->isAdmin = true;

    CHANNEL c;
    strcpy(c.name, name);
    c.admin = *admin;

    c.connected.push_back(*admin);

    channels.push_back(c);
}

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
void sendMessage(char* message, CLIENT client, bool sendAll) {

    mtx.lock();

    //enviar msg dentro de canais

    //p usuario, ver canais que ele entrou e enviar para os clientes que estao nele
    if(sendAll == false){
        printf("entrou pra enviar msg\n");
        CHANNEL ch;
        printf("nome canal cliente: %s\n", client.channelName);
        for (auto i = channels.begin(); i != channels.end(); i++){
            printf("percorrendo canais: %s\n", (*i).name);
            
            if (strcmp((*i).name, client.channelName) == 0){
                ch = (*i);
                printf("esta no canal %s\n", (*i).name);
            }
        }

        for (auto i = ch.connected.begin(); i != ch.connected.end(); i++){
            printf("percorrendo clientes: %s", (*i).nickname);
            if (((*i).usrID != client.usrID) && ((*i).connected == true)){
                printf("vai enviar para %s\n", (*i).nickname);
                bool sended = false;
                int count = 0;

                while(sended == false && count < 5){
                    if (write((*i).socketId, message, strlen(message)) < 0 && flagMsg == false) {                
                        count++;
                    }
                    else sended = true;
                }

                if(count >= 5 && sended == false){
                    error("!!! Error sending a message !!!\n");
                    close((*i).usrID);
                }
            }
        }
    }



    // for(int i = 0; i < MAXCLIENTS; i++){
    //     if((clients[i].usrID != userID && clients[i].connected == true) || (sendAll==true && clients[i].connected == true)){
            
    //         bool sended = false;
    //         int count = 0;

    //         while(sended == false && count < 5){
    //             if (write(clients[i].socketId, message, strlen(message)) < 0 && flagMsg == false) {                
    //                 count++;
    //             }
    //             else sended = true;
    //         }

    //         if(count >= 5 && sended == false){
    //             error("!!! Error sending a message !!!\n");
    //             close(clients[i].usrID);
    //         }
    //     }
    // }

    mtx.unlock();
}

/*
    Function that checks if client is admin in current channel.
*/
bool isAdmin(CLIENT client){
    for (int i = 0; i < channelAmnt; i++)
        if (strcmp (client.channelName, channels[i].name) == 0)
            if (channels[i].admin.usrID == client.usrID)
                return true;
    return false;
}

/*
    Function that checks if client is muted in current channel.
*/
bool isMuted(CLIENT client){
    for (int i = 0; i < channelAmnt; i++)
        if (strcmp (client.channelName, channels[i].name) == 0)
            for (auto j = channels[i].mutedUsers.begin(); j != channels[i].mutedUsers.end(); j++) 
                if ((*j).usrID == client.usrID)
                    return true;

    return false;
}

/* 
    Function that searches for a client in a specific channel
*/
CLIENT getClientByName(CHANNEL channel, char* name){
    for (auto i = channel.connected.begin(); i != channel.connected.end(); i++) 
        if (strcmp((*i).nickname, name) == 0)
            return *i;
        
    // Don't know what to do if nickname doesn't exist?/?
    CLIENT notExists;
    notExists.usrID = -1;
    return notExists;

}


/*
    Function that controls commands from clients.
    Checks if client is allowed to use command.
*/
void clientCommand(char* message, CLIENT *client){

    /* ------- COMMAND /ping ------- */
    if (strcasecmp(message, "/ping") == 0){
        write(client->socketId, "pong", strlen("pong"));     // responds client with pong.
    }


    /* ------- COMMAND /join ------- */
    if (strncasecmp(message, "/join ", 6) == 0){     // connects client to a channel

        char* channelName = (char*)malloc(sizeof(char)*200);
        strcpy(channelName, message + strlen("/join "));
        
        if((channelName[0] == '&' || channelName[0] == '#') && strlen(channelName) <= 200){
            printf("--- Entering the %s ---\n", channelName);
            sendMessage("entering", *client, false);

            bool flag = true;
            for (auto i = channels.begin(); i != channels.end(); i++){
                printf("percorrendo canais: %s", (*i).name);
                if (strcmp((*i).name, channelName) == 0){      // if channel already exists, add client to channel
                    printf("adicionando cliente no canal %s\n", channelName);
                    strcpy (client->channelName, channelName);
                    printf("strcpy pro client chan name: %s\n", client->channelName);
                    client->isAdmin = false;

                    (*i).connected.push_back(*client);        
                    flag = false;
                    break;
                }
            }

            if (flag){
                printf("criando canal %s\n", channelName);
                addChannel(channelName, client); // if channel doesn't exist, create channel
            }

        }
        else{
            char errorMSG[200] = "\n - Error! Try again the command \"/join channelName\"\n - Remembers that channel names must be less than 200 characters and start with '&' or '#'. Also, it must not contain spaces, CTRL + G or commas.\n";
            write(client->socketId, errorMSG, strlen(errorMSG));
        }       

    }
    else if (strncasecmp(message, "/nickname ", 10) == 0 ){ // changes users nickname

        strcpy(client->nickname, message + strlen("/nickname "));
        write(client->socketId, "Nickname changed successfully!", strlen("Nickname changed successfully!"));

    }

    // ##ADMIN COMMANDS##
    else if(strncasecmp(message, "/kick ", 6) == 0){     // kicks another user from channel.
        printf("entrou no kick\n\n");
        if (client->isAdmin){
            char name[NICK_SIZE];
            CHANNEL tempChan;
            strcpy(name, message + strlen("/kick "));
            for (auto i = channels.begin(); i != channels.end(); i++)
               if (strcmp (client->channelName, (*i).name) == 0)
                    tempChan = *i;

            //CLIENT temp = getClientByName(tempChan, name);  // client to be kicked
            CLIENT ck = getClientByName(tempChan, name);
            if(ck.usrID != -1){
                 // remove client
                // ck.connected = false;
                // close(ck.socketId);
                // clientsNum--;
                // flagFull = false;
                write(ck.socketId, "kicked", strlen("kicked"));
                printf("User %s kicked.\n", ck.nickname);
            }
            else printf("user does not exists in this channel\n");

            //Kick client here

        }
        else {
             write(client->socketId, "You are not allowed to use this command!", strlen("You are not allowed to use this command!"));
        }

    }
    else if(strcasecmp(message, "/mute") == 0){ // mutes a user in current channel.
        if (isAdmin(*client)){
            char name[NICK_SIZE];
            CHANNEL tempChan;
            for (auto i = channels.begin(); i != channels.end(); i++)
               if (strcmp (client->channelName, (*i).name) == 0)
                    tempChan = *i;

            strcpy(name, message + strlen("/mute "));
            
            CLIENT temp = getClientByName(tempChan, name);
            tempChan.mutedUsers.push_back(temp);    // adds indicated client to muted list

        }
        else {
             write(client->socketId, "You are not allowed to use this command!", strlen("You are not allowed to use this command!"));
        }
    }
    else if(strcasecmp(message, "/unmute") == 0){   // unmutes a user in current channel.
        if (isAdmin(*client)){
            char name[NICK_SIZE];
            CHANNEL tempChan;
            for (auto i = channels.begin(); i != channels.end(); i++)
               if (strcmp (client->channelName, (*i).name) == 0)
                    tempChan = *i;

            strcpy(name, message + strlen("/unmute "));
            CLIENT temp = getClientByName(tempChan, name);  // gets client to be unmuted

            for (auto i = tempChan.mutedUsers.begin(); i != tempChan.mutedUsers.end(); i++)   // removes client form muted list
                if (strcmp (temp.nickname, (*i).nickname) == 0)
                    tempChan.mutedUsers.erase(i);
                
            
        }
        else {
             write(client->socketId, "You are not allowed to use this command!", strlen("You are not allowed to use this command!"));
        }
    }
    else if(strcasecmp(message, "/whois") == 0){    // returns IP address of a user.
        if (isAdmin(*client)){
            char name[NICK_SIZE];
            CHANNEL tempChan;
             for (int i = 0; i < channelAmnt; i++)
               if (strcmp (client->channelName, channels[i].name) == 0)
                    tempChan = channels[i];
            strcpy(name, message + strlen("/whois "));
            //CLIENT temp = getClientByName(tempChan, name);
            // Get clients IP here
            // Send it back to admin
        }
        else {
             write(client->socketId, "You are not allowed to use this command!", strlen("You are not allowed to use this command!"));
        }
    }

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

        bzero(message, BUFFER_SIZE+NICK_SIZE+2); // clears message

        int receive = recv(client.socketId, message, BUFFER_SIZE+NICK_SIZE+2, 0);

        if(receive > 0){
            if (strlen(message) > 0) {

                if(message[0] == '/')
                    clientCommand(message, &client);

                else{
                    char formatedMessage[BUFFER_SIZE+NICK_SIZE+200];
                    sprintf(formatedMessage, "%s: %s", client.nickname, message);
                    if(client.isAdmin) printf("eh admin!");
                    else printf("no es admin\n");
                    sendMessage(formatedMessage, client, false);
                    
                    // Prints on the server who sent the message to whom
                    printf("%s\n", formatedMessage); 
                }
            }
        }
        else if (receive == 0 || strcasecmp("quit\n", message) == 0) {
            sprintf(message, "\n---- %s has left. ----\n\n", client.nickname);
            printf("%s", message);

            sendMessage(message, client, false);

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
                    
                    CLIENT c;
                    sendMessage(message, c, true);
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
            CLIENT c;
            sendMessage(message, c, true); // send message to all clients
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

                CLIENT c;

                c.address = clientAddress;
                c.socketId = clientSocket;
                c.connected = true;
                c.usrID = clientsNum;

                clients.push_back(c);

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
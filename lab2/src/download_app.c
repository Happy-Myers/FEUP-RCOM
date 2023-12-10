#include "../include/download_app.h"

int main(int argc, char *argv[]){

    if(argc != 2) {
        printf("Usage: ./download_app ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    } 

    struct URL url;
    memset(&url, 0, sizeof(url));
    if(parseURL(argv[1], &url) != 0) {
        printf("Parse error. Usage: ./download_app ftp://[<user>:<password>@]<host>/<url-path>\n");
        exit(-1);
    }

    printf("Host: %s\nResource: %s\nFile: %s\nUser: %s\nPassword: %s\nIP Address: %s\n", url.host, url.resource, url.file, url.user, url.password, url.ip);

    char answer[MAX_LENGTH];
    int socketA = createSocket(url.ip, FTP_PORT);
    if(socketA < 0 || readResponse(socketA, answer) != AUTH_READY){
        printf("Socket to '%s' and port %d failed\n", url.ip, FTP_PORT);
        exit(-1);
    }

    if(confirmAuthentication(socketA, url.user, url.pwd) < 0){
        printf("Authentication failed with username = '%s' and password = '%s'.\n", url.user, url.pwd);
        exit(-1);
    }

    int port;
    char ip[MAX_LENGTH];
    if(passiveMode(socketA, ip, &port) < 0){
        printf("Passive mode failed\n");
        exit(-1);
    }

    int socketB = createSocket(ip, port);
    if(socketB < 0){
        printf("Socket to '%s:%d' failed\n", ip, port);
        exit(-1);
    }

    if(requestResource(socketA, url.resource) < 0) {
        printf("Unknown resouce '%s' in '%s:%d'\n", url.resource, ip, port);
        exit(-1);
    }

    if(getFile(socketA, socketB, url.file) < 0) {
        printf("Error transfering file '%s' from '%s:%d'\n", url.file, ip, port);
        exit(-1);
    }

    if(endConnection(socketA, socketB) < 0) {
        printf("Sockets close error\n");
        exit(-1);
    }

    return 0;
}

int parseURL(char *input, struct URL *url){

    regex_t regex;
    if(regcomp(&regex, BARRA, 0) != 0) return -1;
    if(regexec(&regex, input, 0, NULL, 0)) return -1;

    if(regcomp(&regex, ARROBA, 0) != 0) return -1;
    if(regexec(&regex, input, 0, NULL, 0) != 0){
        sscanf(input, HOST_REGEX, url->host);
        strcpy(url->user, DFLT_USR);
        strcpy(url->pwd, DFLT_PWD);
    }
    else{
        sscanf(input, HOST_ARROBA_REGEX, url->host);
        sscanf(input, USR_REGEX, url->user);
        sscanf(input, PWD_REGEX, url->pwd);
    }

    sscanf(input, RESOURCE_REGEX, url->resource);
    strcpy(url->file, strrchr(input, '/') + 1);

    struct hostent *h;
    if(strlen(url->host) == 0) return -1;
    if((h = gethostbyname(url->host)) == NULL){
        printf("Invalid hostname '%s'\n", url->host);
        return -1;
    }
    strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    return !(
        strlen(url->host) && strlen(url->user) && 
        strlen(url->pwd) && strlen(url->resource) && 
        strlen(url->file));
}

int createSocket(char *ip, int port){
    int sockfd;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);  
    server_addr.sin_port = htons(port); 
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("socket()");
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
        perror("connect()");
        return -1;
    }
    
    return sockfd;
}

int readResponse(const int socket, char* buffer){
    int responseCode;
    memset(buffer, 0, MAX_LENGTH);

    if(stateMachineRead(socket, buffer) < 0) return -1;

    sscanf(buffer, RESPCODE_REGEX, &responseCode);
    return responseCode;
}

int stateMachineRead(const int socket, char* buffer){
    char byte;
    int index = 0;
    ResponseState state = START;

    while(state != END){
        if(read(socket, &byte, 1) < 0){
            perror("Error while reading\n");
            return -1;
        }

        switch(state){
            case START:
                if(byte == ' ') state = SINGLE;
                else if(byte == '-') state = MULTIPLE;
                else if(byte == '\n') state = END;
                else buffer[index++] = byte;
                break;

            case SINGLE:
                if(byte == '\n') state = END;
                else buffer[index++] = byte;
                break;

            case MULTIPLE:
                if(byte == '\n'){
                    memset(buffer, 0, MAX_LENGTH);
                    state = START;
                    index = 0;
                }
                else buffer[index++] = byte;
                break;

            case END:
                break;
        }
    }

    return index;
}

int confirmAuthentication(const int socket, const char* user, const char* pass){
    char userCommand[5+strlen(user)+1], passCommand[5+strlen(pass)+1], answer[MAX_LENGTH];

    // user
    strcpy(userCommand, "user ");
    strcat(userCommand, user);
    strcat(userCommand, "\n");
    printf("%s", userCommand);

    write(socket, userCommand, strlen(userCommand));
    if(readResponse(socket, answer) != PWD_READY){
        printf("Unknown user: '%s'. Aborting.\n", user);
        return -1;
    }

    // password
    strcpy(passCommand, "pass ");
    strcat(passCommand, pass);
    strcat(passCommand, "\n");
    printf("%s", passCommand);
    
    write(socket, passCommand, strlen(passCommand));
    if(readResponse(socket, answer) != LOG_SUCCESS){
        printf("Incorrect password: '%s'. Aborting.\n", pass);
        return -1;
    }

    return 0;
}

int passiveMode(const int socket, char *ip, int *port){
    char answer[MAX_LENGTH];
    int ip1, ip2, ip3, ip4, port1, port2;

    write(socket, "pasv\n", 5);
    if(readResponse(socket, answer) != PASSIVE) return -1;

    sscanf(answer, PASSIVE_REGEX, &ip1, &ip2, &ip3, &ip4, &port1, &port2);
    *port = port1 * 256 + port2;
    snprintf(ip, MAX_LENGTH, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

    return 0;
}

int requestResource(const int socket, char *resource){
    char fileCommand[5+strlen(resource)+1];
    char answer[MAX_LENGTH];
    strcpy(fileCommand, "retr ");
    strcat(fileCommand, resource);
    strcat(fileCommand, "\n");

    write(socket, fileCommand, sizeof(fileCommand));
    if(readResponse(socket, answer) != TRANSFER_READY){
        printf("Error reaching resourse: '%s'. Aborting.\n", resource);
        return -1;
    }
    
    return 0;
}

int getFile(const int socketA, const int socketB, char *filename){
    FILE *fd = fopen(filename, "wb");
    if(fd == NULL){
        printf("Error opening or creating file '%s'\n", filename);
        return -1;
    }

    char buffer[MAX_LENGTH];
    int bytes;
    int eof = TRUE;

    while(eof){
        if((bytes = read(socketB, buffer, MAX_LENGTH)) == 0) eof = FALSE;
        if(fwrite(buffer, bytes, 1, fd) < 0) return -1;
    }

    fclose(fd);

    if(readResponse(socketA, buffer) != TRANSFER_COMPLETE){
        printf("Error transfering resource: '%s'. Aborting.\n", filename);
        return -1;
    }

    return 0;
}

int endConnection(const int socketA, const int socketB){
    char answer[MAX_LENGTH];
    write(socketA, "quit\n", 5);

    if(readResponse(socketA, answer) != END_CONNECTION){
        printf("Error ending connection. Aborting anyway\n");
        return -1;
    }

    return close(socketA) || close(socketB);
}

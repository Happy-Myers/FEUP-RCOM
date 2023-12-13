#include "../include/download_app.h"

extern char response[MAX_LENGTH];

int main(int argc, char *argv[]){

    // ./download ftp://rcom:rcom@netlab1.fe.up.pt/files/pic2.png
    if(argc != 2)
        handleError("Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>");

    URL url;
    memset(&url, 0, sizeof(url));
    if(parseURL(argv[1], &url) != 0)
        handleError("Parse error. Usage: ./download ftp://[<user>:<password>@]<host>/<url-path>");

    printConnParams(url);

    int socketA = createSocket(url.ip, FTP_PORT);
    if(socketA < 0 || readResponse(socketA) != AUTH_READY){
        printf("Socket to '%s' and port %d failed\n", url.ip, FTP_PORT);
        exit(-1);
    }

    if(login(socketA, url.user, url.pwd) < 0){
        printf("Authentication failed with username = '%s' and password = '%s'.\n", url.user, url.pwd);
        exit(-1);
    }

    int port;
    char ip[MAX_LENGTH];
    if(passiveMode(socketA, ip, &port) < 0)
        handleError("Passive mode failed");

    int socketB = createSocket(ip, port);
    if(socketB < 0){
        printf("Socket to '%s:%d' failed\n", ip, port);
        exit(-1);
    }

    if(requestResource(socketA, url.resource) < 0){
        printf("Unknown resouce '%s' in '%s:%d'\n", url.resource, ip, port);
        exit(-1);
    }

    printf("Getting file...\n");
    if(getFile(socketA, socketB, url.file) < 0){
        printf("Error transfering file '%s' from '%s:%d'\n", url.file, ip, port);
        exit(-1);
    }
    printf("File Transfer Complete!\n");

    if(endConnection(socketA, socketB) < 0)
        handleError("Sockets close error\n");

    return 0;
}

int parseURL(char *input, URL *url){

    char* ftp = strtok(input, ":");
    char* args = strtok(NULL, "/");
    char* path = strtok(NULL, "");

    if(ftp == NULL || args == NULL || path == NULL)
        handleError("Couldnt process input as URL");

    getCredentials(args, url);
    getResource(path, url);

    struct hostent *h;
    if(strlen(url->host) == 0 || (h = gethostbyname(url->host)) == NULL)
        handleError("Couldnt get host name.");

    strcpy(url->ip, inet_ntoa(*((struct in_addr *) h->h_addr)));

    return 0;
}

void getCredentials(char* args, URL *url){
    char* login = NULL, *host = NULL, *user = NULL, *pwd = NULL;

    if(strchr(args, '@') != NULL){
        login = strtok(args, "@"); // credenciais user:password
        host = strtok(NULL, "@"); // o restante, nomeadamente o host

        user = strtok(login, ":"); // extrai o username das credenciais
        pwd = strtok(NULL, ":"); // extrai a password das credenciais
    }

    host = (user == NULL && pwd == NULL) ? args : host;
    user = user == NULL ? DFLT_USR : user;
    pwd = pwd == NULL ? DFLT_PWD : pwd;

    strcpy(url->user, user);
    strcpy(url->pwd, pwd);
    strcpy(url->host, host);
}

void getResource(char* path, URL *url){
    // Copy the entire path to the resource field
    strncpy(url->resource, path, MAX_LENGTH - 1);
    url->resource[MAX_LENGTH - 1] = '\0';  // Ensure null-termination

    // Use basename to get the file name
    strncpy(url->file, basename(path), MAX_LENGTH - 1);
    url->file[MAX_LENGTH - 1] = '\0';  // Ensure null-termination
}

int createSocket(char *ip, int port){
    int sockfd;
    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);  
    server_addr.sin_port = htons(port); 
    
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        handleError("Error on socket()");

    if(connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        handleError("Error on connect()");
    
    return sockfd;
}

int readResponse(const int socket){
    int bytes_read = 0;
    size_t n_bytes = 1;
    char buf[MAX_LENGTH];

    memset(response, 0, sizeof(response));
    int code = 0;

    n_bytes = recv(socket, buf, sizeof(buf), 0);
    if(n_bytes < 0) handleError("Error while reading response");

    else{
        strncat(response, buf, n_bytes - 1);
        bytes_read += n_bytes;
        sscanf(buf, "%d", &code);
    }

    printf("response: %s",response);

    return code;
}

int login(const int socket, const char* usr, const char* pwd){
    char userCommand[5+strlen(usr)+1], passCommand[5+strlen(pwd)+1];

    // user
    strcpy(userCommand, "user ");
    strcat(userCommand, usr);
    strcat(userCommand, "\n");

    write(socket, userCommand, strlen(userCommand));
    if(readResponse(socket) != PWD_READY)
        handleErrorObject("Unknown user", usr);

    // password
    strcpy(passCommand, "pass ");
    strcat(passCommand, pwd);
    strcat(passCommand, "\n");
    
    write(socket, passCommand, strlen(passCommand));
    if(readResponse(socket) != LOG_SUCCESS)
        handleErrorObject("Incorrect password", pwd);
    
    return 0;
}

int passiveMode(const int socket, char *ip, int *port){
    int ip1, ip2, ip3, ip4, port1, port2;

    write(socket, "pasv\n", 5);
    if(readResponse(socket) != PASSIVE) return -1;

    if(sscanf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2) != 6)
        handleError("Couldnt parse IP and Port");

    *port = port1 * 256 + port2;
    snprintf(ip, MAX_LENGTH, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);

    return 0;
}

int requestResource(const int socket, char *resource){
    char fileCommand[5+strlen(resource)+1];
    strcpy(fileCommand, "retr ");
    strcat(fileCommand, resource);
    strcat(fileCommand, "\n");

    write(socket, fileCommand, sizeof(fileCommand));
    if(readResponse(socket) != TRANSFER_READY)
        handleErrorObject("Error reaching resourse:", resource);
    
    return 0;
}

int getFile(const int socketA, const int socketB, char *filename){
    FILE *fd = fopen(filename, "wb");

    if(fd == NULL)
        handleErrorObject("Error opening or creating file", filename);

    char buffer[MAX_LENGTH];
    int bytes;
    int eof = FALSE;

    while(!eof){
        if((bytes = read(socketB, buffer, MAX_LENGTH)) == 0) eof = TRUE;

        else if(bytes < 0) handleError("Error reading from socket");

        else{
            if((bytes = fwrite(buffer, bytes, 1, fd)) < 0)
                handleError("Error writing to file");
        }
    }

    fclose(fd);

    if(readResponse(socketA) != TRANSFER_COMPLETE)
        handleErrorObject("Error transfering resource:", filename);

    return 0;
}

int endConnection(const int socketA, const int socketB){
    write(socketA, "quit\n", 5);

    if(readResponse(socketA) != END_CONNECTION)
        handleError("Error ending connection. Aborting anyway\n");

    return close(socketA) || close(socketB);
}

void handleError(const char *errorMessage){
    perror(errorMessage);
    exit(-1);
}

void handleErrorObject(const char *errorMessage, const char* object){
    fprintf(stderr, "%s %s. Closing\n", errorMessage, object);
    exit(-1);
}

void printConnParams(URL url){
    printf("--- Connection Parameters ---\n");
    printf(" -Host: %s\n", url.host);
    printf(" -Resource: %s\n", url.resource);
    printf(" -File: %s\n", url.file);
    printf(" -User: %s\n", url.user);
    printf(" -Password: %s\n", url.pwd);
    printf(" -IP: %s\n", url.ip);
    printf("-----------------------------\n");
}

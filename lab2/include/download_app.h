#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <libgen.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_LENGTH  1024
#define FTP_PORT    21
#define FALSE       0
#define TRUE        1

/* Server responses */
#define AUTH_READY              220
#define PWD_READY               331
#define LOG_SUCCESS             230
#define PASSIVE_1               227
#define PASSIVE_2               125
#define TRANSFER_READY          150
#define TRANSFER_COMPLETE       226
#define END_CONNECTION          221

/* Default credentials*/
#define DFLT_USR        "anonymous"
#define DFLT_PWD        "anonymous"

typedef struct {
    char host[MAX_LENGTH];      // 'netlab1.fe.up.pt'
    char resource[MAX_LENGTH];  // 'path/to/file/pipe.txt'
    char file[MAX_LENGTH];      // 'pipe.txt'
    char user[MAX_LENGTH];      // 'username'
    char pwd[MAX_LENGTH];       // 'password'
    char ip[MAX_LENGTH];        // 193.137.29.15
} URL;

char response[MAX_LENGTH];

int parseURL(char *input, URL *url);
void getCredentials(char* args, URL *url);
void getResource(char* path, URL *url);
int createSocket(char *ip, int port);
int readResponse(const int socket);
int login(const int socket, const char* user, const char* pass);
int passiveMode(const int socket, char *ip, int *port);
int requestResource(const int socket, char *resource);
int getFile(const int socketA, const int socketB, char *filename);
int endConnection(const int socketA, const int socketB);
void handleError(const char *errorMessage);
void handleErrorObject(const char *errorMessage, const char* object);
void printConnParams(URL url);
int checkLastLine(char *message);

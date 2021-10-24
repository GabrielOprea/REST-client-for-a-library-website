#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "include/helpers.h"
#include "include/requests.h"
#include "include/buffer.h"
// Oprea-Groza Gabriel
// 323CB

//Functie care scoate trailing \n ul dintr-un string, daca exista
void remove_trailing_newl(char* string) {
    int len = strlen(string);
    if(string[len - 1] == '\n') {
        string[len - 1] = '\0';
    }
}

//Extrage cookie-ul dintr-un raspuns al serverului
char* extract_cookie(char* response) {
    
    //Daca raspunde cu 400 Bad Request, atunci nu are un cookie atasat
    if(strstr(response, "400 Bad Request")) {
        return NULL;
    }
    char* result = calloc(BUFLEN, sizeof(char));
    if(!result) exit(ERROR);

    int i = 0;

    /* Se cauta campul Set-Cookie si se parcurge
    pana la intalnirea unei noi linii */
    char* flag = "Set-Cookie: ";
    char* p = strstr(response, flag);
    p += strlen(flag);

    while(*p != '\r' && *p != '\n') {
        result[i++] = *p;
        p++;
    }
    result[i] = '\0';

    return result;
}

//Extrage token-ul JWT dintr-un raspuns al serverului
char* extract_jwt(char* response) {
    char* result = calloc(BUFLEN, sizeof(char));
    if(!result) exit(ERROR);

    int i = 0;

    /*Se cauta in raspuns dupa flagul {token:", care delimiteaza
    inceputul tokenului JTW, si se termina la aparitia altui caracter " */
    char* flag = "{\"token\":\"";
    char* p = strstr(response, flag);
    p += strlen(flag);

    while(*p != '\"') {
        result[i++] = *p;
        p++;
    }
    result[i] = '\0';

    return result;
}



int main(int argc, char *argv[])
{
    char* message;
    char* response;
    // Initial clientul nu are cookie sau token de autorizatie salvat
    char* cookie = NULL;
    char* jwt = NULL;

    while(1) {
        //Se reface mereu conexiunea pentru a nu da timeout
        int sockfd = open_connection("34.118.48.238", 8080, AF_INET, SOCK_STREAM, 0);

        //Citesc comanda
        char cmd[BUFLEN];
        fgets(cmd, BUFLEN, stdin);

        remove_trailing_newl(cmd);
        // Trimit cererea corecta la server in functie de comanda
        if (!strcmp(cmd, "register")) {
            //Pentru register, vor fi 2 parametri aditionali: user si parola
            int body_data_fields_count = 2;
            char **body_data = (char**)malloc(body_data_fields_count*(sizeof(char*)));
            if(!body_data) exit(ERROR);
            body_data[0] = (char*) malloc(BUFLEN * sizeof(char));
            body_data[1] = (char*) malloc(BUFLEN * sizeof(char));
            if(!body_data[0] || !body_data[1]) {
                exit(ERROR);
            }

            //Prompt pt user
            printf("username=");
            fgets(body_data[0], BUFLEN, stdin);
            remove_trailing_newl(body_data[0]);
            //Prompt pt parola
            printf("password=");
            fgets(body_data[1], BUFLEN, stdin);
            remove_trailing_newl(body_data[1]);

            /* Obtin POST request-ul cu user si parola pe care il
            trimit la server */ 
            message = compute_post_request("34.118.48.238", "/api/v1/tema/auth/register",
                "application/json", body_data, body_data_fields_count, NULL, 0, REGISTER);
            
            //afisez requestul si il trimit la server
            puts(message);
            send_to_server(sockfd, message);

            //primesc raspunsul si il afisez asa cum este
            response = receive_from_server(sockfd);
            puts(response);

            free(body_data[0]);
            free(body_data[1]);
            free(body_data);
        } else if (!strcmp(cmd, "login")) {
            int body_data_fields_count = 2;
            char **body_data = (char**)malloc(body_data_fields_count*(sizeof(char*)));
            if(!body_data) exit(ERROR);
            body_data[0] = (char*) malloc(BUFLEN * sizeof(char));
            body_data[1] = (char*) malloc(BUFLEN * sizeof(char));
            if(!body_data[0] || !body_data[1]) {
                exit(ERROR);
            } 

            printf("username=");
            fgets(body_data[0], BUFLEN, stdin);
            remove_trailing_newl(body_data[0]);

            printf("password=");
            fgets(body_data[1], BUFLEN, stdin);
            remove_trailing_newl(body_data[1]);

            // Verific sa vad daca am cookie sau nu
            int nr_cookies = 0;
            if(cookie != NULL) {
                nr_cookies = 1;
            }

            /*Daca am cookie, il trimit, altfel body_data_fields_count va fi 0
            si nu se va trimite */
            message = compute_post_request("34.118.48.238", "/api/v1/tema/auth/login",
                "application/json", body_data, body_data_fields_count, &cookie, nr_cookies, LOGIN);

            puts(message);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            puts(response);

            //Salvez si extrag cookie-ul
            cookie = extract_cookie(response);

            free(body_data[0]);
            free(body_data[1]);
            free(body_data);

        } else if (!strcmp(cmd, "enter_library")) {
            int nr_cookies = 0;
            if(cookie != NULL) {
                nr_cookies = 1;
            }
            /*Trimit request-ul la server cu cookie-ul curent. Daca am un
            cookie valid, serverul va accepta si va trimite un JWT token */
            message = compute_get_request("34.118.48.238", "/api/v1/tema/library/access", NULL, 
                &cookie, nr_cookies, ENTER_LIBRARY);
            puts(message);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            puts(response);

            //Daca raspunsul e unul de succes, extrag token-ul din raspuns
            if(nr_cookies != 0) {
                jwt = extract_jwt(response);
            }
        } else if (!strcmp(cmd, "get_books")) {
            //Verific sa vad daca am sau nu autorizatie
            int is_authorised = jwt == NULL ? 0 : 1;

            /*Daca am autorizatie, trimit si token-ul JWT. Altfel trimit un
            token null si serverul imi va da mesaj eroare */
            message = compute_get_request("34.118.48.238", "/api/v1/tema/library/books", NULL,
                &jwt, is_authorised, GET_BOOKS);
            puts(message);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            puts(response);
        } else if (!strcmp(cmd, "get_book")) {
            char* bookId = (char*) malloc(BUFLEN * sizeof(char*));
            if(!bookId) exit(ERROR);
            //Citesc id-ul cartii
            printf("id=");
            fgets(bookId, BUFLEN, stdin);
            remove_trailing_newl(bookId);

            //Folosesc un buffer pentru a forma adresa url finala a cartii
            buffer full_url = buffer_init();
            buffer_add(&full_url,"/api/v1/tema/library/books", strlen("/api/v1/tema/library/books"));
            buffer_add(&full_url,"/", 1);
            buffer_add(&full_url, bookId, strlen(bookId) + 1);
            
            //Daca am autorizatie trimit si tokenul jwt la cerere
            int is_authorised = jwt == NULL ? 0 : 1;
            message = compute_get_request("34.118.48.238", full_url.data, NULL,
                &jwt, is_authorised, GET_BOOK);

            puts(message);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            puts(response);
            free(bookId);

        } else if (!strcmp(cmd, "add_book")) {
            int body_data_fields_count = 5;
            char **body_data = (char**)malloc(body_data_fields_count*(sizeof(char*)));
            if(!body_data) exit(ERROR);
            for(int i = 0; i < body_data_fields_count; i++) {
                body_data[i] = (char*) malloc(BUFLEN * sizeof(char));
                if(!body_data[i]) {
                    exit(ERROR);
                }
            }

            //Prompt pentru citirea tuturor informatiilor despre o carte
            printf("title=");
            fgets(body_data[0], BUFLEN, stdin);
            printf("author=");
            fgets(body_data[1], BUFLEN, stdin);
            printf("genre=");
            fgets(body_data[2], BUFLEN, stdin);
            printf("page_count=");
            fgets(body_data[3], BUFLEN, stdin);
            printf("publisher=");
            fgets(body_data[4], BUFLEN, stdin);

            for(int i = 0; i < body_data_fields_count; i++) {
                remove_trailing_newl(body_data[i]);
            }

            //Daca am autorizatie trimit si tokenul
            int is_authorised = jwt == NULL ? 0 : 1;
            message = compute_post_request("34.118.48.238", "/api/v1/tema/library/books",
                "application/json", body_data, body_data_fields_count, &jwt, is_authorised, ADD_BOOK);

            puts(message);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            puts(response);
            for(int i = 0; i < body_data_fields_count; i++) {
                free(body_data[i]);
            } free(body_data);

        } else if (!strcmp(cmd, "delete_book")) {

            char* bookId = (char*) malloc(BUFLEN * sizeof(char*));
            if(!bookId) exit(ERROR);
            printf("id=");
            fgets(bookId, BUFLEN, stdin);
            remove_trailing_newl(bookId);

            buffer full_url = buffer_init();
            buffer_add(&full_url,"/api/v1/tema/library/books", strlen("/api/v1/tema/library/books"));
            buffer_add(&full_url,"/", 1);
            buffer_add(&full_url, bookId, strlen(bookId) + 1);

            int is_authorised = jwt == NULL ? 0 : 1;
            message = compute_delete_request("34.118.48.238", full_url.data, NULL, &jwt, is_authorised, DELETE_BOOK);

            puts(message);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            puts(response);
            free(bookId);

        } else if (!strcmp(cmd, "logout")) {
            int nr_cookies = 0;
            if(cookie != NULL) {
                nr_cookies = 1;
            }
            /*Trimit cookie-ul la server daca exista, pentru a informa serverul
            ca acesta nu mai este valid */
            message = compute_get_request("34.118.48.238", "/api/v1/tema/auth/logout", NULL,
                &cookie, nr_cookies, LOGOUT);

            puts(message);
            send_to_server(sockfd, message);
            response = receive_from_server(sockfd);
            puts(response);
            
            //Setez cookie-ul si token-ul ca fiind inexistente
            cookie = NULL;
            jwt = NULL;
        } else if (!strcmp(cmd, "exit")) {
            break;
        } else {
            printf("Invalid command!\n");
            continue;
        }
        free(message);
        free(response);
    }
    return 0;
}

#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <stdio.h>
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "include/helpers.h"
#include "include/requests.h"


char* compute_delete_request(char* host, char* url, char* query_params,
                            char** tokens, int token_count, int request_code)
{

    char *message = calloc(BUFLEN, sizeof(char));
    char *line = calloc(LINELEN, sizeof(char));
    if(!message || !line) {
        exit(ERROR);
    }

    /* Scriu numele metodei, URL, parametrii request-ului daca exista si
    numele protocolului utilizat */
    if (query_params != NULL) {
        sprintf(line, "DELETE %s?%s HTTP/1.1", url, query_params);
    } else {
        sprintf(line, "DELETE %s HTTP/1.1", url);
    }

    compute_message(message, line);

    // Adaug hostul
    sprintf(line, "Host: %s", host);
    compute_message(message, line);

    char* type;
    /* Obtin tipul de informatie care trebuie adaugata. Poate fi
    ori un token de authorization sau un cookie */
    if(request_code == DELETE_BOOK) {
        type = "Authorization: Bearer";
    } else {
        type = "Cookie: ";
    }

    if (tokens != NULL) {
        for(int i = 0; i < token_count; i++) {
            sprintf(line, "%s %s", type, tokens[i]);
            compute_message(message, line);
        }
    }

    compute_message(message, "");
    free(line);
    return message;
}

char *compute_get_request(char *host, char *url, char *query_params,
                            char **tokens, int tokens_count, int request_code)
{
    char *message = calloc(BUFLEN, sizeof(char));
    char *line = calloc(LINELEN, sizeof(char));
    if(!message || !line) {
        exit(ERROR);
    }

    /* Scriu numele metodei, URL, parametrii request-ului daca exista si
    numele protocolului utilizat */
    if (query_params != NULL) {
        sprintf(line, "GET %s?%s HTTP/1.1", url, query_params);
    } else {
        sprintf(line, "GET %s HTTP/1.1", url);
    }

    compute_message(message, line);

    // Adaug hostul
    sprintf(line, "Host: %s", host);
    compute_message(message, line);

    char* type;
    if (request_code == ENTER_LIBRARY || request_code == LOGOUT) {
        type = "Cookie:";
    } else if (request_code == GET_BOOK || request_code == GET_BOOKS) {
        type = "Authorization: Bearer";
    }

    /* Daca exista, adaug cookie-urile sau token-ul JWT */
    if (tokens != NULL) {
        for(int i = 0; i < tokens_count; i++) {
            sprintf(line, "%s %s",type, tokens[i]);
            compute_message(message, line);
        }
    }

    // Adaug un newline la final
    compute_message(message, "");
    free(line);
    return message;
}

char *compute_post_request(char *host, char *url, char* content_type, char **body_data,
                            int body_data_fields_count, char **tokens, int tokens_count,
                            int request_code)
{
    char *message = calloc(BUFLEN, sizeof(char));
    char *line = calloc(LINELEN, sizeof(char));
    char *body_data_buffer = calloc(LINELEN, sizeof(char));
    if(!message || !line || !body_data_buffer) {
        exit(ERROR);
    }

    /* Scriu numele metodei, URL, parametrii request-ului daca exista si
    numele protocolului utilizat */
    sprintf(line, "POST %s HTTP/1.1", url);
    compute_message(message, line);
    
    // Adaug hostul
    sprintf(line, "Host: %s", host);
    compute_message(message, line);
    //Adaug headerele necesare
    sprintf(line, "Content-Type: %s", content_type);
    compute_message(message, line);

    //Initializez un obiect JSON ca sa ma folosesc de Parson
    JSON_Value *init_val = json_value_init_object();

    JSON_Object* init_obj = json_value_get_object(init_val);
    int len;
    int ret;

    switch(request_code) {
        case REGISTER:
            //Adaug userul si parola in format JSON
            json_object_set_string(init_obj, "username", body_data[0]);
            json_object_set_string(init_obj, "password", body_data[1]);
            
            // Convertesc din JSON in string
            ret = json_serialize_to_buffer_pretty(init_val, body_data_buffer, LINELEN);
            if(ret == -1) {
                perror("Cannot serialise register request");
                exit(ERROR);
            }
            len = strlen(body_data_buffer);
            break;
        case LOGIN:
            json_object_set_string(init_obj, "username", body_data[0]);
            json_object_set_string(init_obj, "password", body_data[1]);
            ret = json_serialize_to_buffer_pretty(init_val, body_data_buffer, LINELEN);
            if(ret == -1) {
                perror("Cannot serialise login request");
                exit(ERROR);
            }
            len = strlen(body_data_buffer);
            break;
        case ADD_BOOK:
            /*Adaug titlul, autorul, genul, nr de pagini si publisherul ca informatii
            pentru request */
            json_object_set_string(init_obj, "title", body_data[0]);
            json_object_set_string(init_obj, "author", body_data[1]);
            json_object_set_string(init_obj, "genre", body_data[2]);
            json_object_set_string(init_obj, "page_count", body_data[3]);
            json_object_set_string(init_obj, "publisher", body_data[4]);
            ret = json_serialize_to_buffer_pretty(init_val, body_data_buffer, LINELEN);
            if(ret == -1) {
                perror("Invalid login request");
                exit(ERROR);
            }
            len = strlen(body_data_buffer);
    }

    sprintf(line, "Content-Length: %d", len);
    compute_message(message, line);

    // Adaug cookie-uri sau token JWT daca exista
    if (tokens != NULL) {
        for(int i = 0; i < tokens_count; i++) {
            sprintf(line, "Authorization: Bearer %s", tokens[i]);
            compute_message(message, line);
        }
    }

    //Adaug un newline la sfarsit
    compute_message(message, "");

    //Adaug payload-ul
    memset(line, 0, LINELEN);
    compute_message(message, body_data_buffer);

    //Eliberez memoria
    free(line);
    free(body_data_buffer);
    json_value_free(init_val);
    return message;
}

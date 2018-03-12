/*
 * Allie Clifford (acliff01)
 * Comp 112 Homework 1
 * date created: 1/29/2018
 * late modified: 2/5/2018
 *
 * a1.c
 *
 * C program intended to implement a simple socket-based
 * web server, serving HTTP via TCP protocol
 *
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
#include <arpa/inet.h>
#include <time.h>
#include <errno.h> //for debug

/*
 * Global clock variable
 */ 
time_t server_time;

/*
 * struct cli_conn holds client connection information
 * each will behave as a node which will form a linked list  
 * -node_num tracks the node's position in the list
 * -access tracks number of accesses
 * -limit acts as a boolean flag to set rate limit
 * -timeout is the cutoff time for rate limit, acts as expiry check
 */
struct cli_conn {
        short node_num; 
        int access;
        int limit; 
        int timeout; 
        long int time_stamp;  
        char *ip;
        char *city;
        struct cli_conn *next; 
};

/*
 * struct cli_buffer will hold pointers to the linked list
 * of cli_conn nodes. Will also provide some mechanisms for
 * control like tracking node count and total connection counts
 */
struct cli_buffer {
        short node_count; 
        int total_count;
        struct cli_conn *start;
        struct cli_conn *end; 
};
/*
 * Global pointer to the cli_buffer data structure
 */
struct cli_buffer *connections; 

/*
 * Function call definitions
 */
void init_client_buffer();
int call_socket(int port, struct sockaddr_in *sock_in); 
void call_bind(int sock, struct sockaddr *sock_in, int sock_size); 
void call_listen(int sock, int num_conn); 
int call_accept(int sock, struct sockaddr *cli_conn, socklen_t *add_size); 
void process_connection(int sock, struct sockaddr_in *client);
int call_read(int sock, char *buffer, int size);
void parse_req(int sock, char *buffer, char *response, int limit_flag); 
void call_write(int sock, char *response, int size);
int log_ip(char *ip);
struct cli_conn *new_client(char *ip);
int rate_limit(struct cli_conn *ptr, long int time);
void error_page(int sock, char *response);
void launch_home(int sock, char *response);
void launch_about(int sock, char *response);
void launch_timeout(int sock, char *response);
long int current_time();
void get_cli_info(char *list);
char *return_city(char *ip);
void free_buffer();

int main(int argc, char *argv[]){ 
        if(argc != 2){
                fprintf(stderr, "Usage: %s port", argv[0]);
                exit(EXIT_FAILURE);
        }       
        init_client_buffer();     
        int master_socket, client_socket, port_num;
        socklen_t cli_sock_len;
        struct sockaddr_in server_address, client_address;   
        port_num = strtol(argv[1], NULL, 10); 
        master_socket = call_socket(port_num, &server_address);
        call_bind(master_socket, (struct sockaddr *)&server_address, 
                                sizeof(server_address)); 
        call_listen(master_socket, 1);
        cli_sock_len = sizeof(struct sockaddr_in);
        char *c;
        while(1){
                
                client_socket = call_accept(master_socket, 
                                (struct sockaddr *)&client_address, 
                                &cli_sock_len);        
                process_connection(client_socket, &client_address); 
                close(client_socket);
        }
        free_buffer();
        close(master_socket);
        return 0; 
}              

/*
 * Function to process the client connection: calls the call_read() 
 * function, and upon successful read it determines client IP and 
 * launches logging function, which will store client data in 
 * client_buffer. Determines if rate limiting should be applied 
 * by the return value of the log_ip function. Then passes the buffer
 * and response to the parse_req function, which calls the appropriate
 * page launch function, based on the parse of the client request 
 *
 */
void process_connection(int cli_sock, struct sockaddr_in *client) {
        int flag = 0; 
        char buffer[256]; 
        char response[256];
        memset(buffer,'\0',256); 
        memset(response,'\0',256);
        call_read(cli_sock, buffer, 256);
        char cli_ip[INET_ADDRSTRLEN];       
        memset(cli_ip,0,INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &(client->sin_addr), cli_ip, INET_ADDRSTRLEN);
        if(log_ip(cli_ip)) {
                flag = 1;
        }
        ++connections->total_count; 
        parse_req(cli_sock, buffer, response, flag);  
}

/*
 * call_socket creates a new socket and performs the necessary 
 * error handling. It initializes the master_socket struct with
 * the appropriate inet, port, and address information. It then
 * returns the newly created TCP stream socket.
 */
int call_socket(int port, struct sockaddr_in *sock_in){
        int new_sock; 
        new_sock = socket(AF_INET, SOCK_STREAM, 0);
        if(new_sock < 0){
                free_buffer();
                fprintf(stderr,"SOCKET error\n");
                exit(EXIT_FAILURE);
        } 
        memset(sock_in,'\0',sizeof(struct sockaddr_in));
        sock_in->sin_family = AF_INET;
        sock_in->sin_port = htons(port);
        sock_in->sin_addr.s_addr = htonl(INADDR_ANY);
        return new_sock;
}
/*
 * call_bind binds the provided socket to the socket address and performs
 * the necessary error handling 
 */
void call_bind(int sock, struct sockaddr *sock_in, int sock_size){
        int bound = bind(sock, sock_in, sock_size);
        if(bound < 0){
                free_buffer();
                fprintf(stderr,"BIND error\n");
                exit(EXIT_FAILURE);
        }
} 
/*
 * call_listen sets the provided socket to listen and limits the
 * number of possible connections
 */
void call_listen(int sock, int num_conn) {
        listen(sock, num_conn);
}
/*
 * call_accept will accept incoming client connection and perform
 * necessary error handling. It returns the socket of the accepted
 * client connection
 */
int call_accept(int sock, struct sockaddr *cli_conn, socklen_t *add_size){
        int client_sock = accept(sock, cli_conn, add_size);
        if(client_sock < 0){
                free_buffer();
                fprintf(stderr,"ACCEPT error\n");
                exit(EXIT_FAILURE); 
        }
        return client_sock;
}
/*
 * call_read calls the read() function on the provided socket, which
 * writes te incoming bytestream to the provided buffer. It returns
 * the number of bytes written to the buffer. It performs the necesary
 * error handling and returns 0 if the bytestream is empty i.e. nothing
 * more to read
 */
int call_read(int sock, char *buffer, int size){
        int num_bytes = read(sock, buffer, size);
        if (num_bytes < 0) {
                free_buffer();
                 fprintf(stderr, "READ error\n");
                 exit(EXIT_FAILURE);
        }
        return num_bytes;        
}
/*
 * call_write calls the write() function and performs the necessary error
 * handling
 */
void call_write(int sock, char *response, int size){
        int num_bytes = write(sock, response, size);
        if(num_bytes < 0) { 
                free_buffer();
                fprintf(stderr, "WRITE error\n");
                exit(EXIT_FAILURE);
        }
}

/*
 * parse_req function will parse the client request and prepare 
 * the appropriate page to be served by calling functions that write
 * the correct HTTP headers to the response buffer. Most importantly
 * it enforces the rate limit once the limit flag is set in the function
 * that calls parse_req (process_connection)
 *
 * When this function call returns, process_connection continues and
 * the appropriate response will be served to the client 
 */
void parse_req(int sock, char *buffer, char *response, int limit_flag) { 
        char dest[12];
        char compare[12];
        strncpy(dest, &buffer[4],12);
        dest[11] = '\0'; 
        strcpy(compare, "/about.html\0"); 
        if(limit_flag){      
                launch_timeout(sock, response);
        }else if(buffer[4] == 0x2f && buffer[5] == 0x20){
                launch_home(sock, response); 
        }else if(strcmp(dest, compare) == 0){
                launch_about(sock, response); 
        }else{
                error_page(sock, response); 
        }
}
/*
 * new_client will return a new struct cli_conn for each new 
 * connection that is successfully made with the server. It 
 * dynamically allocates memory to each newly created node, and 
 * sets the data members accordingly
 */
struct cli_conn *new_client(char *ip){
        struct cli_conn *new_cli = malloc(sizeof(*new_cli));
        new_cli->node_num = 1;
        new_cli->time_stamp = current_time();
        new_cli->ip = ip; 
        new_cli->access = 1;
        new_cli->city = return_city(ip); 
        new_cli->limit = 0; 
        new_cli->timeout = 0;
        new_cli->next = NULL;
        return new_cli;
}

/*
 * log_ip is the function where client data is parsed from an
 * accepted connection request with two sub-functions: to manage the
 * information stored in client_buffer, and to return a boolean
 * to the calling function (process_connection) to enforce rate limiting.
 * It calls on the rate_limit() function to determine the value of the
 * blooean.
 *
 * If client_buffer is empty, it creates a new node and returns 0.
 * Else it checks the buffer to determine if this is a returning client. 
 * If yes, it calls the rate_limit() function to determine if rate 
 * limiting needs to be enforced, and if yes, the local rate limit flag 
 * is set. It will then update the returning client node data and 
 * position to the head of the list, check to ensure that only 10 nodes 
 * are in the list, removing and freeing the oldest if needed, and will 
 * return a 0 or a 1 based on the value of the local rate_limit flag.
 *
 * If no previous access record is found in the client buffer, 
 * then client treated as new, and a node is created followed by an 
 * update of all pointers. If there are 10 nodes already in the 
 * client_buffer, then the node with the oldest access timestamp 
 * is removed and free'd, and returns 0.  
 */
int log_ip(char *ip) {
        if(connections->start == NULL) {  
                struct cli_conn *new_cli = new_client(ip);
                connections->start = new_cli;
                connections->end = new_cli;
                ++connections->node_count;
                return 0;
        } 
        int node = 0, limit_flag = 0; 
        long int time = current_time(); 
        struct cli_conn *ptr_prev, *ptr;
        ptr_prev = connections->start;
        ptr = connections->start;
        while(ptr != NULL) { 
                if(strcmp(ip,ptr->ip)!=0) {
                        ptr = ptr->next;
                        continue;
                } else { 
                        break;
                }
        }
        //if not currently in the buffer, ptr will be set to NULL
        //if not NULL, determine if rate limiting needs to be set. 
        //Then use ptr and ptr_prev to update node data and
        //list position
        if(ptr != NULL) {
                limit_flag = rate_limit(ptr, time);
                ++ptr->access;
                ptr->time_stamp = time;
                node = ptr->node_num; 
                ptr_prev = connections->start;
                while(ptr_prev->next != NULL && 
                        node != ptr_prev->next->node_num){
                        ++ptr_prev->node_num;
                        ptr_prev = ptr_prev->next; 
                }
                ptr_prev->next = ptr->next;       
                ptr->next = connections->start;
                connections->start = ptr;
                ptr->node_num = 1;
                if(connections->node_count > 10) { 
                        ptr = connections->end;
                        connections->end = ptr_prev;
                        free(ptr); 
                        --connections->node_count;
                }
                return limit_flag;                        
        } else {
        //if IP not in queue... 
                ++connections->node_count;
                struct cli_conn *new_cli = new_client(ip);
                new_cli->next = connections->start;
                connections->start = new_cli;
                ptr = connections->start->next;
                while(ptr->next != NULL){
                        ptr_prev = ptr;
                        ++ptr->node_num;
                        connections->end = ptr;            
                        ptr = ptr->next;
                }
                if(connections->node_count > 10) { 
                        ptr = connections->end;
                        connections->end = ptr_prev;
                        free(ptr); 
                        --connections->node_count;
                }
                return 0;
        }
}
/*
 * rate_limit() function uses the struct cli_conn member data and the
 * current time to determine if rate limiting is to be set. It looks
 * at the struct's rate_limit flag, and if set checks the timeout data
 * to determine if 20 seconds has passed since rate limiting was 
 * enforced. If the timeout is up, then the struct member data are
 * reset. If not, rate limiting will continue. If the struct's rate limit
 * flag is not set, the number of accesses are checked. If there are
 * greater than 5 accesses, the timestamp of the last access is checked
 * against the current time, and if less than 20 seconds has passed, then
 * rate limiting is set.
 */
int rate_limit(struct cli_conn *ptr, long int time){
        int limit_flag = 0;
        if(ptr->limit){ 
                if((time - ptr->timeout) > 0){ 
                        ptr->limit = 0;
                        ptr->timeout = 0;
                        ptr->access = 1; 
                } else {            
                         limit_flag = 1;
                }
        } else if(ptr->access >= 5) {   
                if((time - ptr->time_stamp) <= 20) {  
                        ptr->limit = 1; 
                        limit_flag = 1;
                        ptr->timeout = time+20;
                }                                      
        }
        return limit_flag;
}
/*
 * error_page is called if a non-existent page is requested by a client.
 * if called, it writes a 404 error to the response buffer.
 */
void error_page(int sock, char * response) {
        char *resp =  "HTTP/1.1 404 Not Found \r\n";
        memcpy(response,resp,strlen(resp));
        call_write(sock, response, strlen(response));
}
/*
 * launch_home is called as the default page if no page is 
 * specified by the client. It writes the client_buffer connection info
 * to the response buffer, and then writes the header and the list of 
 * previous connections to the TCP stream. It will display a blank page
 * if there are no connections to report. It also shows the current
 * client's connection, rather than updating the page after
 * the client requests.
 */
void launch_home(int sock, char *response) {
        char list[10*(sizeof(struct cli_conn))]; 
        char *resp = "HTTP/1.1 200 OK\r\nConnection: close\r\n\
Content-type: application/json\r\n\r\n";
        memcpy(response,resp,strlen(resp)); 
        call_write(sock, response, strlen(response));
        get_cli_info(list);
        call_write(sock, list, strlen(list));
}
/*
 * get_cli_info interates through the cient_buffer data structure and
 * pulls the ip, access number, and city for each connection into a PBR 
 * list, that is then used in the launch_home function
 */
void get_cli_info(char *list) {
        char main_info[10*sizeof(struct cli_conn)];
        char *ip, *city, access[2];
        int access_i;
        if(connections->start == NULL) {
                return;
        }
        struct cli_conn *ptr = connections->start;
        strcpy(main_info,"["); 
        int count = connections->node_count;
        while(ptr != NULL && count > 0) {
                ip = ptr->ip;
                access_i = ptr->access; 
                sprintf(access,"%d\0",access_i);
                city = ptr->city;
                strcat(main_info,"{'IP': '");
                strcat(main_info,ip);
                strcat(main_info,"', 'Accesses': ");
                strcat(main_info,access);
                strcat(main_info,", 'City': '");
                strcat(main_info,city);
                strcat(main_info,"'}");
                if(ptr->next != NULL) {
                        strcat(main_info,", ");
                }
                ptr = ptr->next;
                --count;
        }
        strcat(main_info,"]");
        memcpy(list,main_info,sizeof(main_info));
}

/*
 * launch_about is called if the client requests \about.html. It writes
 * some details about me to the response buffer
 */
void launch_about(int sock, char *response) { 
        char *resp = "HTTP/1.1 200 OK\r\nConnection: close\r\n\
Content-type: text/html\r\n\r\n<html><header><h1>About Me</h1> \
</header>\r\n\r\n<body><p>Name:Allie Clifford</p>\r\n \
<p>Favorite plant:Monotropa Uniflora</p></body></html>\r\n";
        memcpy(response, resp, strlen(resp));
        call_write(sock, response, strlen(response));
}       
/*
 * launch_timeout is called if the rate_limit flag is set via the log_ip()
 * function call cascade. It writes the rate limit error message below to 
 * the response buffer. The error message is modeled on the unofficial 
 * "Enhance Your Calm" code taken from version 1 of the Twitter Search 
 * and Trends API when a client is being rate limited. The error message
 * is modified for the needs of this assignment
 */
void launch_timeout(int sock, char *response) { 
        char *resp = "HTTP/1.1 420 Rate Limit User\r\nConnection: close\r\n\r\n";
        memcpy(response, resp, strlen(resp));       
        call_write(sock, response, strlen(response));
}

/*
 * current_time returns the timestamp in seconds
 */
long int current_time() {
        server_time = time(&server_time);
        return server_time;
}       

/*
 * return_city() makes a call to the ipinfo API by modeling a client
 * connection to the api socket created for host: freegeoip.net
 * then a GET request is made, which returns a JSON repsonse for the IP
 * provided. The response is parsed and the city is stored in the 
 * cli_conn node being created. If the API is down, then a string
 * indicating an inability to connect is returned in place of the city.
 */
char *return_city(char *ip){
        char *city, buffer[1000]; 
        memset(buffer,'\0', 1000);
        int ip_len = strlen(ip);
        char *api_base = "GET /json/";
        int api_len = strlen(api_base);  
        char *header_end = " HTTP/1.1\r\nHost: freegeoip.net\r\n\r\n";
        int header_len = strlen(header_end);
        char *api_call = malloc(ip_len+api_len+header_len);
        memcpy(api_call, api_base,api_len);
        memcpy(&api_call[api_len],ip,ip_len);
        memcpy(&api_call[api_len+ip_len],header_end,header_len);
        int api_socket;
        struct sockaddr_in api_addr; 
        struct addrinfo hints, *res, *ptr;
        memset(&hints,0,sizeof(hints));
        hints.ai_family=AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        hints.ai_protocol = 0;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;
        const char *url = "freegeoip.net";
        int check = getaddrinfo(url,"http",&hints,&res);
        if(check !=0) {
                free_buffer();
                fprintf(stderr,"error %s\n", gai_strerror(check));
                exit(EXIT_FAILURE);
        }
        api_socket = socket(AF_INET, SOCK_STREAM, 0);
        memset(&api_addr, 0,sizeof(struct sockaddr_in));
        api_addr.sin_family = AF_INET;
        ptr=res;  
        check = connect(api_socket, ptr->ai_addr, ptr->ai_addrlen); 
        if(check < 0) {
                //error handling for inability to connect to API
                fprintf(stderr,"CONNECT TO API error\n"); 
                return "error with freegeoip.net connection";
        }
        call_write(api_socket,api_call, 57); 
        call_read(api_socket, buffer, 1000);
        //iterate through returned buffer until 'cit' is found, then
        //set copy flag and copy until c = '"'
        int i, j = 0, copy_flag = 0;
        char c, temp[100];
        for(i = 0; i < strlen(buffer); i++){
                if(buffer[i] == 0x63 && buffer[i+1] == 0x69 
                        && buffer[i+2] == 0x74) {
                copy_flag = 1;
        }
                if(copy_flag) {
                        c = buffer[i+7];         
                        if(c == '"') break;
                        temp[j] = c;             
                        j++;
                }
        }
        city = malloc(j+1);
        memcpy(city,temp,j);  
        city[j] = '\0';
        free(api_call);
        return city;
}

/*
 * Function to initialize client buffer data structure and set server_time
 */
void init_client_buffer(){
        server_time = time(&server_time);        
        connections = malloc(sizeof(*connections));
        connections->node_count = 0;
        connections->total_count = 0;
        connections->start = NULL;
        connections->end = NULL;
}

/*
 * free_buffer() is called to free all memory allocated for the 
 * client connection buffer at the end of the program, or if any
 * error causes an exit failure
 *
 */
void free_buffer(){
        struct cli_conn *ptr_del, *ptr;
        ptr_del  = connections->start;
        ptr = connections->start;
        while(ptr != NULL) {
                ptr = ptr->next;
                free(ptr_del);
                ptr_del = ptr;
        }
        connections->start = NULL;
        connections->end = NULL;
        free(connections);
}

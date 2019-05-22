#include <cctype>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <sys/wait.h>
#include <unistd.h>
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>
#include "connection.cpp"

using namespace std;


//dictionary file to be used
char *file_name;

//Message to be received or sent by server
char* message;

char *dictionary;
//port to be used by clients
int port_number;

/*
 * Socket Descriptor array with size of space and front and last indices for a bounded buffer
 * Used to determine when to service a client
 */
int client_buffer_size = 10;
int socket_fd[10];
int client_front = 0, client_last = 0;

/*
 * Log Buffer with size of space and front and last indices for a bounded buffer
 * Used to determine when to write the log buffer
 */
char *log_buffer[40];
int log_buffer_size = 40;
int log_front = 0, log_last = 0;

//Initializer the thread pool
pthread_t thread_pool[5];
pthread_t log_thread;

/*
 * Condition Variables to for the client connection buffer and the log buffer
 * Each buffer has a CV to signal/wait for the buffer not being empty/full
 */
pthread_cond_t clients_full;
pthread_cond_t clients_empty;
pthread_cond_t log_full;
pthread_cond_t log_empty;

/*
 * Mutexes for each buffer. Ensures mutual exclusion for that particular buffer
 */
pthread_mutex_t client_mutex;
pthread_mutex_t log_mutex;




int launch(int argc, char* argv[]){
    file_name = "dictionary.txt";
    port_number = 8888;

    if(argc == 1){
        cout << "Dictionary: DEFAULT (dictionary.txt)\nPort: DEFAULT (8888)\n";
    }
    else if (argc == 2){
        if(isdigit(argv[1][0])){
            port_number = atoi(argv[1]);
            cout << "Dictionary: DEFAULT\n";
            cout << "Port: " << argv[1] << '\n';
        }
        else{
            file_name = argv[1];
            cout << "Dictionary: " << argv[1] << '\n';
            cout << "Port: DEFAULT (8888)\n";
        }
    }
    else{
        if(isdigit(argv[1][0])){
            port_number = atoi(argv[1]);
            cout << "Dictionary: " << argv[2] << '\n';
            cout << "Port: " << argv[1] << '\n';
        }
        else{
            file_name = argv[1];
            cout << "Dictionary: " << argv[1] << '\n';
            cout << "Port: " << argv[2] << '\n';
        }
    }

    return 1;
}

//Load dictionary file into memory
char* load_file_buffer(char* file_name){
    char *string = (char*) malloc(1000000*sizeof(char*));
    FILE *pointer;
    pointer = fopen(file_name, "r");
    if(pointer == NULL){
        printf("ERROR: File not opened.\n");
        exit(3);
    }
    int i = 0;
    while((string[i++] = getc(pointer))!=EOF);
    string[i] = '\0';
    fclose(pointer);
    cout << "Dictionary loaded\n";
    return string;
}

//Set up the server
int connect(int port_number,int socket_fd[]){
    //set up connection to client
    int socket_desc,new_socket,c;
    struct sockaddr_in server{},client{};

    //Create socket (create active socket descriptor
    socket_desc = socket(AF_INET,SOCK_STREAM,0);
    if(socket_desc == -1){
        puts("Error creating socket!");
    }

    //prepare sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port_number);

    if(bind(socket_desc,(struct sockaddr*)&server, sizeof(server)) < 0){
        puts("Error: Bind Failed");
        return 1;
    }

    puts("Bind done");

    //
    listen(socket_desc,1);

    //
    puts("Waiting for incoming connections...");

    //
    while(true){

        c = sizeof(struct sockaddr_in);
        new_socket = accept(socket_desc, (struct sockaddr*)&client, (socklen_t*)&c);
        if (new_socket < 0){
            perror("Error: Accept Failed");
            return 1;
        }
        pthread_mutex_lock(&client_mutex);
        while((client_last-client_front)==client_buffer_size){
            pthread_cond_wait(&clients_empty,&client_mutex);
        }

        //put the new socket id onto the buffer
        message = "The server has received your connection. You will be handled shortly.\n";
        write(new_socket,message,strlen(message));
        socket_fd[client_last%client_buffer_size] = new_socket;

        client_last++;

        puts("Connection accepted ");
        cout << "w/ ID: " << new_socket << '\n';

        pthread_cond_signal(&clients_full);
        pthread_mutex_unlock(&client_mutex);

    }

}

void* logger_thread(void*){
    pthread_mutex_lock(&log_mutex);

    //Take a word out of the buffer
    while(log_last == log_front){
        pthread_cond_wait(&log_full,&log_mutex);
    }
    //open file
    FILE *log;
    log = fopen("log.txt","a");

    while(log_buffer[log_front%log_buffer_size] != NULL){
        //put word and correctness in a log file.
        fprintf(log,"%s",log_buffer[log_front%log_buffer_size]);
        log_front++;

        if(log_last == log_front){
            break;
        }
    }

    //close file
    fclose(log);

    pthread_mutex_unlock(&log_mutex);
    pthread_cond_signal(&log_empty);
}

/*
 * Thread function for worker threads to handle clients
 * Thread takes the socket descriptor from front of the queue then checks the word.
 * If the word is correct or not, put that word on the log buffer to be logged by a logger thread.
 * Return to the client whether or not the word was spelled correctly based on the dictionary file.
 */
void* checker_threads(void*){

    //String that states the correctness of the word, result is correctness concatenated with the word sent
    pthread_mutex_lock(&client_mutex);
    while(client_front == client_last){
        pthread_cond_wait(&clients_full,&client_mutex);
    }

    //ID of the socket being worked on
    int id = socket_fd[client_front%client_buffer_size];
    client_front++;
    pthread_mutex_unlock(&client_mutex);
    pthread_cond_signal(&clients_empty);
    char word[2000];
    char* correctness = "";
    memset(word,0,strlen(word));
    while(1) {


        message = "Please enter a word to be checked\n";
        write(id,message,strlen(message));

        while (recv(id, word, 25, 0) > 0) {

            if(strlen(word) <=1){
                continue;
            }
            remove_if(begin(word),end(word), [](unsigned char x){return std::isspace(x);});

            char *result = strstr(dictionary, word);

            if (result != nullptr) {
                cout << "good";
                correctness = " : OK!\n";
                write(id, "OK!\n", strlen("OK!\n"));
            }
            else{
                correctness = " : Misspelled\n";
                write(id, "Mispelled\n", strlen("Mispelled"));
            }

            //Send correctness back to client


            //Add the correctness of the word to the word
            strcat(word,correctness);

            log_buffer[log_last%log_buffer_size] = (char*)calloc(strlen(word),1);
            //Add the word and its correctness to the log buffer
            pthread_mutex_lock(&log_mutex);
            while((log_last-log_front) == log_buffer_size){
                pthread_cond_wait(&log_empty,&log_mutex);
            }

            // add word to buffer
            log_buffer[log_last%log_buffer_size] = word;
            log_last++;

            //release lock
            pthread_mutex_unlock(&log_mutex);
            pthread_cond_signal(&log_full);
            pthread_create(&log_thread,NULL,logger_thread,NULL);
            pthread_join(log_thread,NULL);


            //Free up word
            memset(word,0,strlen(word));


        }
        //Get rid of socket id if the client disconnects
        if(recv(id, word, 25, 0)==0){
            close(id);
            cout << "Client " << id << "disconnected\n";
            break;
        }
    }
}


//main function of server
int main(int argc, char* argv[]) {

    // Launch with arguments
    launch(argc,argv);

    //load dictionary into buffer
    dictionary = load_file_buffer(file_name);

    //initialize the condition variables
    pthread_cond_init(&clients_empty, nullptr);
    pthread_cond_init(&clients_full, nullptr);
    pthread_cond_init(&log_empty, nullptr);
    pthread_cond_init(&log_full, nullptr);



    for(int i = 0;i<sizeof(thread_pool);i++){

        //Create threads for the thread pool, initialized to wait
        pthread_create(&thread_pool[i], nullptr,checker_threads, nullptr);

    }

    //allow connections to server
    connect(port_number,socket_fd);

    for(int i = 0;i<sizeof(thread_pool);i++){
        pthread_join(thread_pool[i], nullptr);
    }

    cout << "All threads finished\n";


    return 0;
}
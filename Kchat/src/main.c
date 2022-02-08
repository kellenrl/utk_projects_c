//File: main.c for lab9 - Threaded Chat Server
//Author: Kellen Leland
//
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include "dllist.h"
#include "jrb.h"
#include "sockettome.h"

typedef struct ChatRoom {
    char name[256];
    pthread_t tid;
    pthread_mutex_t *lock;
    pthread_cond_t *msg_recvd;
    Dllist user_list;
    Dllist msg_list;
} Room;

typedef struct User {
    char name[256];
    pthread_t tid;
    int fd;
    FILE* fin;
    FILE* fout;
    Room* chat_room;
} User;

typedef struct SharedData {
    JRB chat_rooms;
} Shared;

void * room_thread(void *);
void * user_thread(void *);

Shared *shared;

int
main(int argc, char **argv)
{
    int i, fd, port, sock;
    User *user;

    //error check the command line args
    if(argc < 3) {
        fprintf(stderr, "usage: chat_server port Chat-Room-Names ...\n");
        exit(1);
    }

    //create a SharedData struct for sharing between threads
    shared = (Shared *)malloc(sizeof(Shared));
    if(shared == NULL) {
        perror("malloc()");
        exit(1);
    }
    shared->chat_rooms = make_jrb();

    //get all the chat room names from the command line args
    for(i = 2; i < argc; i++) {
        Room* room = (Room *)malloc(sizeof(Room));
        if(room == NULL) {
            perror("malloc()");
            exit(1);
        }
        strcpy(room->name, argv[i]);
        room->user_list = new_dllist();

        //create mutex lock and conditional variable
        room->lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
        if(room->lock == NULL) {
            perror("malloc()");
            exit(1);
        }
        room->msg_recvd = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
        if(room->msg_recvd == NULL) {
            perror("malloc()");
            exit(1);
        }
        pthread_mutex_init(room->lock, NULL);
        pthread_cond_init(room->msg_recvd, NULL);

        //add the room to the shared data tree
        jrb_insert_str(shared->chat_rooms, room->name, new_jval_v((void *)room));

        //fork a thread for each room
        if(pthread_create(&room->tid, NULL, room_thread, room) != 0) {
            perror("pthread_create()");
            exit(1);
        }
    }
    port = atoi(argv[1]);
    sock = serve_socket(port);

    //prints server start confirmation msg
    printf("Chat server started on port: %d\n", port);

    //wait for connections and make user threads
    while(1) {
        fd = accept_connection(sock);
        user = (User *)malloc(sizeof(User));
        if(user == NULL) {
            perror("malloc()");
            exit(1);
        }
        user->fd = fd;
        if(pthread_create(&user->tid, NULL, user_thread, user) != 0) {
            perror("pthread_create()");
            exit(1);
        }
    }
    return(0);
}

//name - room_thread
//brief - function to run room threads 
//param[in] - Void * - pointer to the Room struct
void * 
room_thread(void *r) 
{
    pthread_detach(pthread_self());
    Room *room = (Room *)r;
    Dllist tlist1, tlist2;
    char *msg;

    pthread_mutex_lock(room->lock);
    while(1) {
        room->msg_list = new_dllist();
        while(dll_empty(room->msg_list)) {
            pthread_cond_wait(room->msg_recvd, room->lock);
        }
        //once there is a msg recvd signal above, send all msgs in queue
        dll_traverse(tlist1, room->msg_list) {
            if(tlist1 == NULL) break;
            msg = tlist1->val.s;
            dll_traverse(tlist2, room->user_list) {
                User *user = (User *)tlist2->val.v;
                fputs(msg, user->fout);
                fflush(user->fout);
            }
        }
        dll_traverse(tlist1, room->msg_list){
            free(tlist1->val.s);
        } 
        free_dllist(room->msg_list);
    }
    pthread_mutex_unlock(room->lock);

    //thread finished, clean up memory and exit thread
    Dllist tlist;
    dll_traverse(tlist, room->user_list) {
        User *user = (User *)tlist->val.v;
        fclose(user->fin);
        fclose(user->fout);
        close(user->fd);
        free(user);
    }
    dll_traverse(tlist, room->msg_list) {
        free(tlist->val.s);
    }
    free_dllist(room->msg_list);
    free_dllist(room->user_list);
    free(room->msg_recvd);
    free(room->lock);
    free(room);
    pthread_exit(NULL);
    return(NULL);
}

//name user_thread
//brief - function to run user threads
//param[in] - Void * - pointer to the User struct
void *
user_thread(void *u) 
{
    pthread_detach(pthread_self());
    User *user = (User *)u;
    char buf[4354];
    char msg[4096];
    char *check;
    JRB tmp;
    Dllist tlist;

    //fdopen the sockets
    user->fin = fdopen(user->fd, "r");
    if(user->fin == NULL) {
        perror("fdopen()");
        exit(1);
    }
    user->fout = fdopen(user->fd, "w");
    if(user->fout == NULL) {
        perror("fdopen()");
        exit(1);
    }

    //print each room and its users
    fputs("Chat Rooms:\n", user->fout);
    fputs("\n", user->fout);
    fflush(user->fout);
    
    jrb_traverse(tmp, shared->chat_rooms) {
        Room *room = (Room *)tmp->val.v;
        strcpy(buf, room->name);
        strcat(buf, ":");

        pthread_mutex_lock(room->lock);
        //put all user names in buf for printing
        dll_traverse(tlist, room->user_list) {
            User *temp_user = (User *)tlist->val.v;
            strcat(buf, " ");
            strcat(buf, temp_user->name);
        }
        pthread_mutex_unlock(room->lock);

        strcat(buf, "\n");
        fputs(buf, user->fout);
        fflush(user->fout);
    }

    //get users name
    fputs("\n", user->fout);
    fputs("Enter your chat name (no spaces):\n", user->fout);
    fflush(user->fout);

    check = fgets(user->name, 256, user->fin);
    user->name[strcspn(user->name, "\n")] = '\0';
    //user exited, clean up memory and exit thread
    if(check == NULL) {
        fclose(user->fin);
        fclose(user->fout);
        close(user->fd);
        free(user);
        pthread_exit(NULL);
    }

    //get users chat room
    user->chat_room = NULL; 
    while(user->chat_room == NULL) {
        fputs("Enter chat room:\n", user->fout);
        fflush(user->fout);

        check = fgets(buf, 4354, user->fin);
        buf[strcspn(buf, "\n")] = '\0';
        //user exited, clean up memory and exit thread
        if(check == NULL) {
            fclose(user->fin);
            fclose(user->fout);
            close(user->fd);
            free(user);
            pthread_exit(NULL);
        }
        //find room and put user on the list/assign users chat room
        jrb_traverse(tmp, shared->chat_rooms) {
            Room *room = (Room *)tmp->val.v;
            //find the right room and add use
            if(strcmp(room->name, buf) == 0) {
                pthread_mutex_lock(room->lock);
                dll_append(room->user_list, new_jval_v((void *)user));
                pthread_mutex_unlock(room->lock);
                user->chat_room = room;
            }
        }
        if(user->chat_room == NULL) {
            fputs("No chat room ", user->fout);
            fputs(buf, user->fout);
            fputs("\n", user->fout);
            fflush(user->fout);
        }
    } 	
    strcpy(buf, user->name);
    strcat(buf, " has joined\n");
    //put user joined msg in chat room list and send msg_recvd signal
    pthread_mutex_lock(user->chat_room->lock);
    dll_append(user->chat_room->msg_list, new_jval_s(strdup(buf)));
    pthread_cond_signal(user->chat_room->msg_recvd);
    pthread_mutex_unlock(user->chat_room->lock);

    //loops for user input
    while(fgets(msg, 4096, user->fin) != NULL) {
        if(strcmp(msg, "\n") == 0) continue;
        strcpy(buf, user->name);
        strcat(buf, ": ");
        strcat(buf, msg);
        //put user's msg in chat room list and send msg_recvd signal
        pthread_mutex_lock(user->chat_room->lock);
        dll_append(user->chat_room->msg_list, new_jval_s(strdup(buf)));
        pthread_cond_signal(user->chat_room->msg_recvd);
        pthread_mutex_unlock(user->chat_room->lock);
    }
    //user left, remove from list
    pthread_mutex_lock(user->chat_room->lock);
    dll_traverse(tlist, user->chat_room->user_list) {
        User *temp_user = (User *)tlist->val.v;
        if(strcmp(user->name, temp_user->name) == 0) {
            dll_delete_node(tlist);
            break;
        }
    }
    //send user left msg
    strcpy(buf, user->name);
    strcat(buf, " has left\n");
    dll_append(user->chat_room->msg_list, new_jval_s(strdup(buf)));
    pthread_mutex_unlock(user->chat_room->lock);
    pthread_cond_signal(user->chat_room->msg_recvd);

    //thread finished, clean up memory and exit thread
    fclose(user->fin);
    fclose(user->fout);
    close(user->fd);
    free(user);
    pthread_exit(NULL);
    return(NULL);
}
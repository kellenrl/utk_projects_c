// File: main.c for lab7 -> jsh (part 3)
// Author: Kellen Leland
//
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "fields.h"
#include "jrb.h"
#include "jval.h"

void process_fork(IS, int, int[2]);
void check_fds(int[2]);
void dup2_and_close(int[2]);
int find_redirects(IS, int[2]);
int find_pipes(IS is, int[MAXFIELDS]);
int process_pipes(int[2], int[2], char*, char**);
void close_pipes(int[2]);
void pipe_copy(int[2], int[2]);
void process_wait_jrb(JRB);

#define NO_REDIRECT -1

int 
main(int argc, char** argv) 
{
    IS is;
    JRB wait_jrb;
    char *prompt;
    int noprompt = 0;
    int first_redirect = NO_REDIRECT;

    wait_jrb = make_jrb();
    is = new_inputstruct(NULL);

    //get custom prompt if provided
    if(argc > 1) {
        prompt = malloc(strlen(argv[1]) * sizeof(char));
        strcpy(prompt, argv[1]);
    }
    //otherwise use default prompt
    else {
        prompt = malloc(5 * sizeof(char));
        strcpy(prompt, "jsh: ");
    }
    //if argv[1] == "-" do not print a prompt
    if(strcmp(prompt, "-") == 0) noprompt = 1;
    //print first prompt
    if(noprompt == 0) printf("%s", prompt);

    //loop while getting input from user and while input is not "exit" or CNTRL-D triggered
    while(get_line(is) >= 0 && !feof(stdin)) {
        int has_pipes;
        int pipe_tracker[MAXFIELDS] = {0};
        int my_fd[2] = {STDIN_FILENO, STDOUT_FILENO};
        int my_pipe_fd1[2] = {-1, -1};
        int my_pipe_fd2[2] = {-1, -1};

        //0 for no ampersand, 1 for ampersand
        int ampersand = 0;
        //blank line was entered into jsh, reset prompt
        if(is->NF == 0) {
            if(noprompt == 0) printf("%s", prompt);
            continue;
        }
        //if user inputs exit, deallocate memory and exit the shell
        else if(strcmp(is->fields[0], "exit") == 0) {
            free(prompt);
            jettison_inputstruct(is);
            jrb_free_tree(wait_jrb);
            exit(0);
        }
        //set last field to null for use in execvp
        is->fields[is->NF] = NULL;
        //check if last arg is & and handle if so
        if(strcmp(is->fields[is->NF - 1], "&") == 0) {
            ampersand = 1;
            is->fields[is->NF - 1] = NULL;      
        }
        //check line for pipes
        has_pipes = find_pipes(is, pipe_tracker);

        //line has pipes
        if(has_pipes == 1) {
            if(pipe(my_pipe_fd2) < 0) {
                perror("pipe()");
                exit(1);
            }

            //check for redirects
            first_redirect = find_redirects(is, my_fd);
            int temp_pipe_fd1[2] = {my_fd[0], -1};
            int temp_pipe_fd2[2] = {-1, my_fd[1]};

            //this is the first command on the line
            //process pipes and fork()
            int pipertn = process_pipes(temp_pipe_fd1, my_pipe_fd2, is->fields[0], is->fields); 
            jrb_insert_int(wait_jrb, pipertn, new_jval_v(NULL));
            close_pipes(temp_pipe_fd1);

            //processes inner commands if exists, not first or last command
            int i;
            for(i = 1; i < pipe_tracker[0]; i++) {
                close_pipes(my_pipe_fd1);
                pipe_copy(my_pipe_fd1, my_pipe_fd2);

                if(pipe(my_pipe_fd2) < 0) {
                    perror("pipe()");
                    exit(1);
                }

                //process pipes and fork()
                pipertn = process_pipes(my_pipe_fd1, my_pipe_fd2, is->fields[pipe_tracker[i]], &is->fields[pipe_tracker[i]]); 
                jrb_insert_int(wait_jrb, pipertn, new_jval_v(NULL));
            }

            //processes last command
            close_pipes(my_pipe_fd1);
            pipe_copy(my_pipe_fd1, my_pipe_fd2);

            //process pipes and fork()
            pipertn = process_pipes(my_pipe_fd1, temp_pipe_fd2, is->fields[pipe_tracker[i]], &is->fields[pipe_tracker[i]]); 
            jrb_insert_int(wait_jrb, pipertn, new_jval_v(NULL));

            close_pipes(my_pipe_fd1);
            close_pipes(my_pipe_fd2);
            close_pipes(temp_pipe_fd1);
            close_pipes(temp_pipe_fd2);

            if (ampersand == 1) {
                process_wait_jrb(wait_jrb);
            }
        }
        //line has no pipes
        else {
            //reset my file descriptors to std fd's to process new line
            my_fd[0] = STDIN_FILENO;
            my_fd[1] = STDOUT_FILENO;
            //find first redirect and process redirects into my file descriptors my_fd[2]
            first_redirect = find_redirects(is, my_fd);

            //if we found a file redirection, set first occurence to NULL
            if(first_redirect != NO_REDIRECT) {
                is->fields[first_redirect] = NULL;
            }

            //call fork() and process children
            process_fork(is, ampersand, my_fd);

            //prints prompt if needed
            if(noprompt == 0) printf("%s", prompt);
        }
    }

    free(prompt);
    jettison_inputstruct(is);
    jrb_free_tree(wait_jrb);

    return(0);
}

//name - pipe_copy
//brief - copies second pipe arg into first pipe arg
//param[in] - pipe1 - destination pipe
//param[in] - pipe2 - source pipe
//param[out] - pipe1 == pipe2
void 
pipe_copy(int pipe1[2], int pipe2[2]) 
{
    pipe1[0] = pipe2[0];
    pipe1[1] = pipe2[1];
}

//name - close_pipes
//brief - closes pipes held in my_pipe[2] if they are custom pipes
//param[in] - my_pipe[2] - if these are custom pipes they are now closed
void 
close_pipes(int my_pipe[2]) 
{
    if(my_pipe[0] > 2) close(my_pipe[0]);
    if(my_pipe[1] > 2) close(my_pipe[1]);
}

//name - process_wait_jrb
//brief - goes through wait_jrb tree waiting for child processes and removes them from tree
//param[in] - wait_jrb - tree with the child process ids to wait on
void 
process_wait_jrb(JRB wait_jrb) 
{
    int status, waitrtn;
    JRB temp_node;

    while(!jrb_empty(wait_jrb)){
        waitrtn = wait(&status);
        temp_node = jrb_find_int(wait_jrb, waitrtn);

        //memory deallocation
        if(temp_node != NULL) {
            free(temp_node->val.v);
            jrb_delete_node(temp_node);
        }
    }
}

//name find_pipes
//brief - goes through the provided line, finds and processes any pipes
//param[in] - IS is - is for the current line
//param[in] - pipe_tracker[] - holds number of pipes at [0] and NF number of pipes after that
//post - is now has NULL fields where any pipe was found
int 
find_pipes(IS is, int pipe_tracker[MAXFIELDS]) 
{
    int i, has_pipes, npipes;
    int j = 1;

    for(i = 0; i < is->NF; i++) {
        if(is->fields[i] != NULL) {
            if(strcmp(is->fields[i], "|") == 0) {
                pipe_tracker[j] = i + 1;
                is->fields[i] = NULL;
                j++;
                npipes++;
                has_pipes = 1;
            }
        }
    }
    pipe_tracker[0] = npipes;
    return(has_pipes);
}

//name process_pipes
//brief - takes pipe fds and is->fields execvp args and processes commands in the child process
int 
process_pipes(int my_pipe_fd1[], int my_pipe_fd2[], char *field, char **fields) 
{
    int forkrtn = fork();

    //if we are in the child process
    if(forkrtn == 0) {
        //if this is not a standard fd
        if(my_pipe_fd1[0] > 2) {
            if(dup2(my_pipe_fd1[0], 0) < 0) {
                perror("dup2()");
                exit(1);
            }
        }
        close_pipes(my_pipe_fd1);

        //if this is not a standard fd
        if(my_pipe_fd2[1] > 2) {
            if(dup2(my_pipe_fd2[1], 1) < 0) {
                perror("dup2()");
                exit(1);
            }
        }
        close_pipes(my_pipe_fd2);

        execvp(field, fields);
        perror(field);
        exit(1);
    }
    return(forkrtn);
}

//name - process_fork
//brief - calls fork() and processes children
//param[in] - IS is - input structure that holds the current line
//param[in] - int ampersand - if 0 wait, if 1 do not wait 
//param[in] - int my_fd[2] - my file descriptors
void 
process_fork(IS is, int ampersand, int my_fd[2]) 
{
    int status;
    int forkrtn = fork();

    //if we are in the child process
    if(forkrtn == 0) {
        //if we have custom file decriptors from redirects, dup2() them to stdin/out and close unused fd
        dup2_and_close(my_fd);
        //exec the current process in the child
        execvp(is->fields[0], is->fields);
        perror(is->fields[0]);
        exit(1);
    }
    //if there is no ampersand we need to wait until we return to parent process
    else if(ampersand == 0) {
        check_fds(my_fd);

        //wait until we are back to the parent process
        int waitrtn = wait(&status);
        while(forkrtn != waitrtn) {
            waitrtn = wait(&status);
        }
    }
    //if there is an ampersand we do not need to wait
    else if(ampersand == 1) {
        check_fds(my_fd);
    }
}

//name - dup2_and_close
//brief - checks if these are redirect file decriptors and processes if so
//param[in] - int my_fd[2] - my file descriptors
void 
dup2_and_close(int my_fd[2]) 
{
    if(my_fd[0] != STDIN_FILENO) {
        if(dup2(my_fd[0], STDIN_FILENO) < 0) {
            perror("dup2_and_close():");
            exit(1);
        }
        close(my_fd[0]);
    }
    if(my_fd[1] != STDOUT_FILENO) {
        if(dup2(my_fd[1], STDOUT_FILENO) < 0) {
            perror("dup2_and_close():");
            exit(1);
        }
        close(my_fd[1]);
    }
}

//name - check_fds
//brief - checks if these are redirect file descriptors, closes if so
//param[in] - int my_fd[2] - my file descriptors
void 
check_fds(int my_fd[2]) 
{
    //if these are not the std file descriptors, close them
    if(my_fd[0] != STDIN_FILENO) {
        close(my_fd[0]);
    }
    if(my_fd[1] != STDOUT_FILENO) {
        close(my_fd[1]);
    }
}

//name - find_redirects
//brief - search input line for redirections >, >>, < abd processes
//param[in] - IS is - the input structure
//param[in] - int my_fd[2] - my file descriptors
//return - the is->field number of the first redirect found or NO_REDIRECT
int 
find_redirects(IS is, int my_fd[2]) 
{
    int i;
    int found = 0;
    int first_redirect = NO_REDIRECT;

    for(i = 0; i < is->NF; i++) {

        if(is->fields[i] != NULL) {
            if(strcmp(is->fields[i], ">") == 0) {
                if(found == 0){
                    first_redirect = i;
                    found = 1;
                }
                is->fields[i] = NULL;
                int my_fd_out = open(is->fields[i+1], O_WRONLY | O_TRUNC | O_CREAT, 0644);
                if(my_fd_out < 0) {
                    perror("my_fd_out >");
                    exit(1);
                }
                my_fd[1] = my_fd_out;
            }
            else if(strcmp(is->fields[i], ">>") == 0) {
                if(found == 0){
                    first_redirect = i;
                    found = 1;
                }
                is->fields[i] = NULL;
                int my_fd_out = open(is->fields[i+1], O_WRONLY | O_APPEND | O_CREAT, 0644);
                if(my_fd_out < 0) {
                    perror("my_fd_out >>");
                    exit(1);
                }
                my_fd[1] = my_fd_out;
            }
            else if(strcmp(is->fields[i], "<") == 0) {
                if(found == 0){
                    first_redirect = i;
                    found = 1;
                }
                is->fields[i] = NULL;
                int my_fd_in = open(is->fields[i+1], O_RDONLY, 0644);
                if(my_fd_in < 0) {
                    perror("my_fd_in <");
                    exit(1);
                }
                my_fd[0] = my_fd_in;
            }
        }
    }
    //if no file redirects found, return redirect
    if(first_redirect == NO_REDIRECT) {
        return(NO_REDIRECT);
    } 
    //else return the location of the first redirect in the is->fields array
    else {
        return(first_redirect);
    } 
}
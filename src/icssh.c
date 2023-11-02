#include "icssh.h"
#include "linkedlist.h"
#include <readline/readline.h>
#include <signal.h>
#include <sys/stat.h>

volatile bool reapingTime = false;
volatile bool killingTime = false;


void SigChildHandler(int sig) {
    reapingTime = true;
}
void SigUser2Handler(int sig) {
    killingTime = true;    
}


int main(int argc, char* argv[]) {
    int max_bgprocs = -1;
	int exec_result;
	int exit_status;
	pid_t pid;
	pid_t wait_result;
	char* line;
    // Create a linked list for background processes
    list_t* bgList = CreateList(BgComparator, BgPrinter, DeleteBgProcess); 
    signal(SIGCHLD, SigChildHandler);
    signal(SIGUSR2, SigUser2Handler);


#ifdef GS
    rl_outstream = fopen("/dev/null", "w");
#endif

    // check command line arg
    if(argc > 1) {
        int check = atoi(argv[1]);
        if(check != 0)
            max_bgprocs = check;
        else {
            printf("Invalid command line argument value\n");
            exit(EXIT_FAILURE);
        }
    }

	// Setup segmentation fault handler
	if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
		perror("Failed to set signal handler");
		exit(EXIT_FAILURE);
	}
    char prevDirectory[256];
    bool thereIsPrevDirectory = false;
    	// print the prompt & wait for the user to enter commands string
	while ((line = readline(SHELL_PROMPT)) != NULL) {
        	// MAGIC HAPPENS! Command string is parsed into a job struct
        	// Will print out error message if command string is invalid
		    job_info* job = validate_input(line);
        	if (job == NULL) { // Command was empty string or invalid
			free(line);
			continue;
		}

        	//Prints out the job linked list struture for debugging
        	#ifdef DEBUG   // If DEBUG flag removed in makefile, this will not longer print
            		debug_print_job(job);
        	#endif
        
        if(reapingTime) {
            while((pid = waitpid(-1, &exit_status, WNOHANG)) > 0) {
                RemoveJobFromList(bgList, pid);
            }
            reapingTime = false;
        }
        if(killingTime) {
            time_t currTime;
            time(&currTime);
            fprintf(stderr, "%s\n", ctime(&currTime));
            killingTime = false;
        }


		// Built-in Functions: exit, cd, estatus
		if (strcmp(job->procs->cmd, "exit") == 0) {
			// Terminating the shell
            node_t* p = bgList->head;
            while(p != NULL) {
                node_t* next = p->next;
                printf(BG_TERM, (((bgentry_t*)p->data)->pid),(((bgentry_t*)p->data)->job->line));
                kill((((bgentry_t*)p->data)->pid), SIGUSR2);
                free_job(((bgentry_t*)p->data)->job);
                free(((bgentry_t*)p->data));
                RemoveFromHead(bgList);
                p = next;
            }
            free(bgList);
            free(line);
            free_job(job);
            validate_input(NULL);   // calling validate_input with NULL will free the memory it has allocated
            return 0;
		}
        

        if(strcmp(job->procs->cmd, "cd") == 0) { // user typed cd
            char buffer[256];
            if(job->procs->argc == 1) {
                chdir(getenv("HOME"));
                printf("%s\n", getcwd(buffer, 256));
            }
            else if(job->procs->argc == 2 && strcmp(job->procs->argv[1], "-") == 0 && thereIsPrevDirectory) {
                if(chdir(prevDirectory) != 0) {
                    fprintf(stderr, DIR_ERR);
                } 
                printf("%s\n", getcwd(buffer, 256));
            }
            else {
                strcat(prevDirectory, getcwd(buffer, 256));
                thereIsPrevDirectory = true;
                char* cmdCommand = job->procs->argv[1];
                if(chdir(cmdCommand) != 0 ) {
                    fprintf(stderr, DIR_ERR);
                }
                else {
                    printf("%s\n", getcwd(buffer, 256));
                }

            }
            free_job(job);
            free(line);
            continue;
        }
 
        if((strcmp(job->procs->argv[0], "estatus") == 0) && job->procs->argc == 1) {
            printf("%d\n", WEXITSTATUS(exit_status));
            free_job(job);
            free(line);
            continue;
        }
        if((strcmp(job->procs->argv[0], "bglist") == 0) && job->procs->argc == 1) {
            PrintLinkedList(bgList, stderr);
            free_job(job);
            free(line);
            continue;
        }

        if((strcmp(job->procs->argv[0], "ascii53")) == 0 && job->procs->argc == 1) {
            printf("Call pipe(p)!\n");
            printf("\nPipe Created !\n");
            printf("\n=================              =====================\n");
            printf("|  |             \\            /                 |  |\n");
            printf("|  |              \\          /                  |  |\n");
            printf("|  |               \\        /                   |  |\n");
            printf("|  |         	     =======                    |  |\n");
            printf("|  |	   Write Pipe            Read Pipe  	|  |\n");
            printf("|  |                                            |  |\n");
            printf("|  |                                            |  |\n");
            printf("|  |                                            |  |\n");
            printf("|  |                =========                   |  |\n");
            printf("|  |               /         \\                  |  |\n");
            printf("|  |              /           \\                 |  |\n");
            printf("==================             ======================\n");
            free_job(job);
            free(line);
            continue;
        } 

        if((strcmp(job->procs->argv[0], "fg") == 0) && job->procs->argc == 1) { // Run the most recent foreground process 
            // Steps: Take the head of the linked list and remove it. 
            if(bgList->head == NULL) {
                fprintf(stderr,PID_ERR); 
                free_job(job);
                free(line);
                continue;
            }
            node_t *n = (bgList->head);
            pid = ((bgentry_t *)(n->data))->pid;
            char* jobLine = ((bgentry_t*)(n->data))->job->line;
            printf("%s\n", jobLine);
            RemoveFromHead(bgList);
                
            wait_result = waitpid(pid, &exit_status, 0);
            if (wait_result < 0) {
                printf(WAIT_ERR);
                free_job(job);
                free(line);
                exit(EXIT_FAILURE);
            }
            free_job(job);
            free(line);
            continue;
 
        }   
         else if((strcmp(job->procs->argv[0], "fg") == 0) && job->procs->argc == 2) { // Run a specific PID
            // Steps: Find the Job in the linked list and remove it.
            pid = atoi(job->procs->argv[1]);
            bgentry_t* fgJob = FindEntryByPID(bgList, pid);
            if(fgJob == NULL || pid == 0) {
                fprintf(stderr,PID_ERR);
            }
            else {
                char* jobLine = fgJob->job->line;
                printf("%s\n", jobLine);
                RemoveJobFromList(bgList, pid);
            }
            wait_result = waitpid(pid, &exit_status, 0);
            if (wait_result < 0) {
                printf(WAIT_ERR);
                exit(EXIT_FAILURE);
            }
            free_job(job);
            free(line);
            continue;
                
        }
        
            
            

        if(job->nproc > 1) { 
            pid_t c1PID; 
            int numProcesses = job->nproc;
            proc_info* proc = job->procs;
            pid_t children[numProcesses - 1];
            int prevRead = 0;
            
            for(int i = 0; i < numProcesses; i++) {
                int p[2]; // p[1] write: p[0] read
                pipe(p);
                c1PID = fork();
                children[i] = c1PID;
                if(c1PID == 0) { // echo
                    if(i == numProcesses - 1) {
                        dup2(prevRead, STDIN_FILENO);
                    }
                    else {
                        dup2(prevRead, STDIN_FILENO);
                        dup2(p[1], STDOUT_FILENO);
                    }    
                    exec_result = execvp(proc->cmd, proc->argv);

                    if (exec_result < 0) {  //Error checking
                        printf(EXEC_ERR, proc->cmd);
                        // Cleaning up to make Valgrind happy
                        // (not necessary because child will exit. Resources will be reaped by parent)
                        free_job(job);
                        free(line);
                        validate_input(NULL);  // calling validate_input with NULL will free the memory it has allocated
                        exit(EXIT_FAILURE);
                    } 
                }
                else { // Parent Process
                    proc = proc->next_proc;
                    if(prevRead != 0) {
                        close(prevRead);
                    }
                    prevRead = p[0];
                    close(p[1]);
                }
            }
            for(int i = 0; i < numProcesses; i++) {
                wait_result = waitpid(children[i], &exit_status, 0);
                if (wait_result < 0) {
                    printf(WAIT_ERR);
                    exit(EXIT_FAILURE);
                }
            }
            free_job(job);  // if a foreground job, we no longer need the data
            free(line);
        }
        else {
        //===================
		// example of good error handling!
        // create the child proccess
        
		if ((pid = fork()) < 0) {
            free_job(job);
            free(line);
			perror("fork error");
			exit(EXIT_FAILURE);
		}

        // Begin the Child Processes for background process commands 
        

		if (pid == 0) {  //If zero, then it's the child process
            //get the first command in the job list to execute
             proc_info* proc = job->procs;
            if(job->in_file != NULL || job->out_file != NULL || proc->err_file != NULL) {
                if(   (job->in_file   != NULL && job->out_file != NULL && !strcmp(job->in_file,   job->out_file))   || 
                      (proc->err_file != NULL && job->in_file  != NULL && !strcmp(proc->err_file, job->in_file ))   || 
                      (proc->err_file != NULL && job->out_file != NULL && !strcmp(proc->err_file, job->out_file)) )
                {
                    fprintf(stderr, RD_ERR);
                    free_job(job);
                    free(line);
                    continue;
                }
                else {
                    if(job->in_file != NULL) {
                        // This is for the < operator we read from the inputfile 
                        int fdin = open(job->in_file, O_RDONLY);
                        if(fdin == -1) {
                            fprintf(stderr, RD_ERR);
                            free_job(job);
                            free(line);
                            continue;
                        }
                        else {
                            dup2(fdin, STDIN_FILENO);    
                        }
                    }
                    if(job->out_file != NULL) {
                        // > operator: 
                        int fdout = open(job->out_file, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
                        dup2(fdout, STDOUT_FILENO);
                    }
                    if(proc->err_file != NULL) {
                        //  2> redirects output to stderr file
                        int fderr = open(proc->err_file, O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
                        dup2(fderr, STDERR_FILENO);
                    }
                }
            }



            exec_result = execvp(proc->cmd, proc->argv);
			if (exec_result < 0) {  //Error checking
				printf(EXEC_ERR, proc->cmd);
				// Cleaning up to make Valgrind happy 
				// (not necessary because child will exit. Resources will be reaped by parent)
				free_job(job);  
				free(line);
    			validate_input(NULL);  // calling validate_input with NULL will free the memory it has allocated

				exit(EXIT_FAILURE);
			} 
		} 
        else {
            
            // PART 2 : Background Jobs
            if(bgList->length == max_bgprocs && job->bg == true) {
                fprintf(stderr, BG_ERR);
                free_job(job);
                free(line);
                continue;
            }
            else if(job->bg == true){
                time_t seconds = time(NULL);
                bgentry_t* bgJob = (bgentry_t*) malloc(sizeof(bgentry_t));
                bgJob->job = job;
                bgJob->pid = pid;
                bgJob->seconds = seconds;
                InsertInOrder(bgList, bgJob);
                free(line);
                continue;
            }

        	// As the parent, wait for the foreground job to finish
			wait_result = waitpid(pid, &exit_status, 0);
           	if (wait_result < 0) {
				printf(WAIT_ERR);
				exit(EXIT_FAILURE);
			}

		}
       
		free_job(job);  // if a foreground job, we no longer need the data
		free(line);
	    }
    }
    

    	// calling validate_input with NULL will free the memory it has allocated
    	validate_input(NULL);



#ifndef GS
	fclose(rl_outstream);
#endif
	return 0;
}

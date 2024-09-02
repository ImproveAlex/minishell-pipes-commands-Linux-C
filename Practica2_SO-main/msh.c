//P2-SSOO-23/24

//  MSH main file
// Write your msh source code here

//#include "parser.h"
#include <stddef.h>			/* NULL */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_COMMANDS 8

int num_executed_commands = 0;
int aux_num_executed_commands = 0;

// files in case of redirection
char filev[3][64];

//to store the execvp second parameter
char *argv_execvp[8];

void siginthandler(int param)
{
	printf("****  Exiting MSH **** \n");
	//signal(SIGINT, siginthandler);
	exit(0);
}

/* mycalc */
void mycalc(char* op1, char* operador, char* op2){

    int operando_1 = atoi(op1);
    int operando_2 = atoi(op2);
    int resultado;
    int resto;

    char* acc_str = getenv("Acc");
    int acc = acc_str ? atoi(acc_str) : 0;

    if (strcmp(operador, "add") == 0){
        resultado = operando_1 + operando_2;
        acc += resultado;
        char acc_str_new[sizeof(acc)];
        sprintf(acc_str_new, "%d", acc);
        setenv("Acc", acc_str_new, 1);
        fprintf(stderr,"[OK] %d + %d = %d; Acc %s\n", operando_1, operando_2, resultado, getenv("Acc"));
    } else if (strcmp(operador, "mul") == 0){
        resultado = operando_1 * operando_2;
        fprintf(stderr,"[OK] %d * %d = %d \n", operando_1, operando_2, resultado);
    } else if (strcmp(operador, "div") == 0){
        resultado = operando_1 / operando_2;
        resto = operando_1 % operando_2;
        fprintf(stderr,"[OK] %d / %d = %d; Resto %d \n", operando_1, operando_2, resultado, resto);
    } else {
        printf("[ERROR] La estructura del comando es mycalc <operando_1> <add/mul/div> <operando_2>\n");
    }
}

/* myhistory */

typedef struct
{
  // Store the number of commands in argvv
  int num_commands;
  // Store the number of arguments of each command
  int *args;
  // Store the commands
  char ***argvv;
  // Store the I/O redirection
  char filev[3][64];
  // Store if the command is executed in background or foreground
  int in_background;
}command;

command commands[20];

void print_commands(command commands[]) {
    int i, j, k;
    for (i = 0; i < num_executed_commands; i++) {
        fprintf(stderr, "%d ", i);
        for (j = 0; j < commands[i].num_commands; j++) {
            for (k = 0; k < commands[i].args[j]; k++) {
                fprintf(stderr, "%s ", commands[i].argvv[j][k]);
            }
            if (j < commands[i].num_commands - 1) {
                fprintf(stderr, "| ");
            }
        }
        if (commands[i].in_background) {
            fprintf(stderr, "&");
        }
        for (int f = 0; f < 3; f++) {
            if (strcmp(commands[i].filev[f], "0") != 0) {
                switch(f) {
                    case 0:
                        fprintf(stderr, "< %s ", commands[i].filev[f]);
                        break;
                    case 1:
                        fprintf(stderr, "> %s ", commands[i].filev[f]);
                        break;
                    case 2:
                        fprintf(stderr, "!> %s ", commands[i].filev[f]);
                        break;
                }
            }
        }
        fprintf(stderr, "\n");
    }
}

void print_one_command(command commands[], int index) {
    if (index < 0 || index >= num_executed_commands) {
        printf("ERROR: Comando no encontrado\n");
    } else {
        fprintf(stderr, "Ejecutando el comando %d\n", index);
    }
}

int history_size = 20;
struct command * history;
int head = 0;
int tail = 0;
int n_elem = 0;

void free_command(command *cmd)
{
    if((*cmd).argvv != NULL)
    {
        char **argv;
        for (; (*cmd).argvv && *(*cmd).argvv; (*cmd).argvv++)
        {
            for (argv = *(*cmd).argvv; argv && *argv; argv++)
            {
                if(*argv){
                    free(*argv);
                    *argv = NULL;
                }
            }
        }
    }
    free((*cmd).args);
}

void store_command(char ***argvv, char filev[3][64], int in_background, command* cmd)
{
    int num_commands = 0;
    while(argvv[num_commands] != NULL){
        num_commands++;
    }

    for(int f=0;f < 3; f++)
    {
        if(strcmp(filev[f], "0") != 0)
        {
            strcpy((*cmd).filev[f], filev[f]);
        }
        else{
            strcpy((*cmd).filev[f], "0");
        }
    }

    (*cmd).in_background = in_background;
    (*cmd).num_commands = num_commands-1;
    (*cmd).argvv = (char ***) calloc((num_commands) ,sizeof(char **));
    (*cmd).args = (int*) calloc(num_commands , sizeof(int));

    for( int i = 0; i < num_commands; i++)
    {
        int args= 0;
        while( argvv[i][args] != NULL ){
            args++;
        }
        (*cmd).args[i] = args;
        (*cmd).argvv[i] = (char **) calloc((args+1) ,sizeof(char *));
        int j;
        for (j=0; j<args; j++)
        {
            (*cmd).argvv[i][j] = (char *)calloc(strlen(argvv[i][j]),sizeof(char));
            strcpy((*cmd).argvv[i][j], argvv[i][j] );
        }
    }
}


/**
 * Get the command with its parameters for execvp
 * Execute this instruction before run an execvp to obtain the complete command
 * @param argvv
 * @param num_command
 * @return
 */
void getCompleteCommand(char*** argvv, int num_command) {
	//reset first
	for(int j = 0; j < 8; j++)
		argv_execvp[j] = NULL;

	int i = 0;
	for ( i = 0; argvv[num_command][i] != NULL; i++)
		argv_execvp[i] = argvv[num_command][i];
}



/**
 * Main sheell  Loop  
 */
int main(int argc, char* argv[])
{
    /**** Do not delete this code.****/
	int end = 0; 
	int executed_cmd_lines = -1;
	char *cmd_line = NULL;
	char *cmd_lines[10];

	if (!isatty(STDIN_FILENO)) {
		cmd_line = (char*)malloc(100);
		while (scanf(" %[^\n]", cmd_line) != EOF){
			if(strlen(cmd_line) <= 0) return 0;
			cmd_lines[end] = (char*)malloc(strlen(cmd_line)+1);
			strcpy(cmd_lines[end], cmd_line);
			end++;
			fflush (stdin);
			fflush(stdout);
		}
	}

	/*********************************/

	char ***argvv = NULL;
	int num_commands;

	history = (struct command*) malloc(history_size *sizeof(command));
	int run_history = 0;

	while (1) 
	{
		int status = 0;
		int command_counter = 0;
		int in_background = 0;
		signal(SIGINT, siginthandler);

		if (run_history)
    {
        run_history=0;
    }
    else{
        // Prompt 
        write(STDERR_FILENO, "MSH>>", strlen("MSH>>"));

        // Get command
        //********** DO NOT MODIFY THIS PART. IT DISTINGUISH BETWEEN NORMAL/CORRECTION MODE***************
        executed_cmd_lines++;
        if( end != 0 && executed_cmd_lines < end) {
            command_counter = read_command_correction(&argvv, filev, &in_background, cmd_lines[executed_cmd_lines]);
        }
        else if( end != 0 && executed_cmd_lines == end)
            return 0;
        else
            command_counter = read_command(&argvv, filev, &in_background); //NORMAL MODE
    }
		//************************************************************************************************


		/************************ STUDENTS CODE ********************************/
	   if (command_counter > 0) {
			if (command_counter > MAX_COMMANDS){
				printf("ERROR: El nº máximo de comandos es %d \n", MAX_COMMANDS);
			}
			else {
				//print_command(argvv, filev, in_background);
            
                //Comandos internos o mandatos simples
                if (command_counter == 1){
                    //Comandos internos
                    if ((strcmp(argvv[0][0], "mycalc") == 0) || (strcmp(argvv[0][0], "myhistory") == 0)){
                        //Si no tiene redirecciones y no se ejecuta en background
                        if ((strcmp(filev[0],"0") == 0) && (strcmp(filev[1],"0") == 0) && (strcmp(filev[2],"0") == 0) && (in_background == 0)){
                            //mycalc
                            if (strcmp(argvv[0][0], "mycalc") == 0){
                                //Estructura mycalc: mycalc <operando_1> <add/mul/div> <operando_2>
                                if (argvv[0][1] != NULL && argvv[0][2] != NULL && argvv[0][3] != NULL && argvv[0][4] == NULL){
                                    mycalc(argvv[0][1], argvv[0][2], argvv[0][3]);
                                }
                                //Error en la estructura mycalc
                                else{
                                    printf("ERROR: Estructura incorrecta. mycalc <operando_1> <add/mul/div> <operando_2>\n");
                                }
                            }
                            //myhistory
                            else{
                                //myhistory con argumentos
                                if (argvv[0][1] != NULL){
                                    print_one_command(commands, atoi(argvv[0][1]));
                                //myhistory sin argumentos
                                } else {
                                    print_commands(commands);
                                }
                            }
                        }
                        //Si tiene redirecciones o se ejecuta en background
                        else {
                            printf("ERROR: Los comandos internos no admiten ( < , > , !> , & )\n");
                        }
                    }
                    //Mandatos simples
                    else{
                        pid_t pid;
                        int status;
                        pid = fork();
                        switch (pid){
                            case -1:
                                printf("ERROR fork()");
                                exit(-1);
                                break;
                            //Hijo
                            case 0:
                                int fd;
                                //Redirección de entrada (<)
                                if (strcmp(filev[0], "0") != 0){
                                    close(STDIN_FILENO);
                                    if ((fd = open(filev[0], O_RDONLY)) == -1){
                                        printf("ERROR open() redirección de entrada\n");
                                        exit(-1);
                                    }
                                    if (dup(fd) == -1){
                                        printf("ERROR dup() redirección de entrada\n");
                                        exit(-1);
                                    }
                                } 
                                //Redirección de salida (>)
                                if (strcmp(filev[1], "0") != 0){
                                    close(STDOUT_FILENO);
                                    if ((fd = open(filev[1], O_CREAT | O_WRONLY | O_TRUNC, 0666)) == -1){
                                        printf("ERROR open() redirección de salida\n");
                                        exit(-1);
                                    }
                                    if (dup(fd) == -1){
                                        printf("ERROR dup() redirección de salida\n");
                                        exit(-1);
                                    }
                                }
                                //Redirección de salida de error (!>)
                                if (strcmp(filev[2], "0") != 0){
                                    close(STDERR_FILENO);
                                    if ((fd = open(filev[2], O_CREAT | O_WRONLY | O_TRUNC, 0666)) == -1){
                                        printf("ERROR open() redirección de salida de error\n");
                                        exit(-1);
                                    }
                                    if (dup(fd) == -1){
                                        printf("ERROR dup() redirección de salida de error\n");
                                        exit(-1);
                                    }
                                }
                                //Ejecución de los mandatos
                                execvp(argvv[0][0], argvv[0]);
                                perror("ERROR execvp() mandatos simples");
                                exit(-1);
                                break;
                            //Padre
                            default:
                                //No se ejecuta en &
                                if (in_background == 0){
                                    while (wait(&status) != pid);
                                } 
                                //Si se ejecuta en &
                                else {
                                    printf("Proceso en segundo plano. PID: %d\n",pid);
                                }
                                break;
                        }
                    }
                } 
                //Secuencias de mandatos
                else {
                    int i, p0, status, fd;
                    int p[2];
                    pid_t pid;
                    //Nº de mandatos
                    for (i = 0; i < command_counter; i++){
                        //1. Creación de pipes (excepto para el último mandato)
                        if (i != (command_counter - 1)){
                            if (pipe(p) == -1){
                                printf("ERROR pipe()");
                                exit(-1);
                            }
                        }
                        //2. fork()
                        pid = fork();
                        switch(pid){
                            case -1:
                                printf("ERROR fork()");
                                break;
                            //Hijo
                            case 0:
                                //Si no es el primer mandato
                                if (i != 0){
                                    close(STDIN_FILENO);
                                    if (dup(p0) == -1){
                                        printf("ERROR dup() hijo si no es el primer mandato");
                                        exit(-1);
                                    }
                                    close(p0);
                                }
                                //Si no es el último mandato
                                if (i != (command_counter - 1)){
                                    close(STDOUT_FILENO);
                                    if (dup(p[1]) == -1){
                                        printf("ERROR dup() hijo si no es el último mandato");
                                        exit(-1);
                                    }
                                    close(p[1]);
                                    close(p[0]);
                                }
                                //Si es el primer mandato
                                if (i == 0){
                                    //Redirección de entrada (<)
                                    if (strcmp(filev[0], "0") != 0){
                                        close(STDIN_FILENO);
                                        if ((fd = open(filev[0], O_RDONLY)) == -1){
                                            printf("ERROR open() redirección de entrada");
                                            exit(-1);
                                        }
                                        if (dup(fd) == -1){
                                            printf("ERROR dup() redirección de entrada");
                                            exit(-1);
                                        }
                                    } 
                                }
                                //Si es el último mandato
                                if (i == (command_counter - 1)){
                                    //Redirección de salida (>)
                                    if (strcmp(filev[1], "0") != 0){
                                        close(STDOUT_FILENO);
                                        if ((fd = open(filev[1], O_CREAT | O_WRONLY | O_TRUNC, 0666)) == -1){
                                            printf("ERROR open() redirección de salida");
                                            exit(-1);
                                        }
                                        if (dup(fd) == -1){
                                            printf("ERROR dup() redirección de salida");
                                            exit(-1);
                                        }
                                    }
                                    //Redirección de salida de error (!>)
                                    if (strcmp(filev[2], "0") != 0){
                                        close(STDERR_FILENO);
                                        if ((fd = open(filev[2], O_CREAT | O_WRONLY | O_TRUNC, 0666)) == -1){
                                            printf("ERROR open() redirección de salida de error");
                                            exit(-1);
                                        }
                                        if (dup(fd) == -1){
                                            printf("ERROR dup() redirección de salida de error");
                                            exit(-1);
                                        }
                                    }
                                }
                                //Ejecución de los mandatos
                                execvp(argvv[i][0], argvv[i]);
                                perror("ERROR execvp() secuencias de mandatos");
                                exit(-1);
                                break;
                            //Padre
                            default:
                                //Si no es el último mandato
                                if (i != (command_counter - 1)){
                                    close(p[1]);
                                    p0 = p[0];
                                //Si es el último mandato
                                } else {
                                    close(p[0]);
                                }
                                break;
                        }
                    }
                    //Si no se ejecuta en &
                    if (in_background == 0){
                        while (wait(&status) != pid);
                    } 
                    //Si se ejecuta en &
                    else {
                        printf("Proceso en segundo plano. PID: %d\n",pid);
                    }
                }   
			}
		}
        if (num_executed_commands < 20){
            store_command(argvv, filev, in_background, &commands[num_executed_commands]);
            num_executed_commands++;
        } else {
            store_command(argvv, filev, in_background, &commands[aux_num_executed_commands]);
            aux_num_executed_commands = (aux_num_executed_commands + 1) % 20;
        }
    }
	return 0;
}

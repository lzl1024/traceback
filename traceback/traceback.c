/** @file traceback.c
 *  @brief The traceback function
 *
 *  This file contains the traceback function for the traceback library
 *
 *  @author Harry Q. Bovik (hqbovik)
 *  @bug No known bugs
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include "traceback_internal.h"

/* maximum number of char to print */
#define MAX_STRING_LEN 25
/* muximum number of element to print */
#define MAX_ARRAY_LEN 3

/**
 * @brief  Print the trace back information of the function.
 *         This is the meat part of this assignment.
 *
 * @param  fp The file stream to which the stack trace should
 *         be printed
 * @return Void.  
 */
void traceback(FILE *fp);

/**
 * @brief  Get the %ebp when entering traceback function
 *
 * @return ebp on the current stack.  
 */
int* trace_init_ebp();

/**
 * @brief  Get the index from the functions array givin the return
 *         address of the function
 *
 * @param  reutrn_address The return address of the function
 * @return The index in the functions list  
 */
int get_index(int return_address);

/**
 * @brief  Print the arguments of the function
 * 
 * Go through the argument list and print them according to their type 
 *
 * @param  fp The file stream to which the stack trace should
 *         be printed
 * @param  function The function to print arguments
 * @param  ebp The basic stack pointer of the function 
 * @return Void.
 */
void print_arguments(FILE *fp, functsym_t function, int* ebp);

/**
 * @brief  Print the argument whose type is char*
 *
 * Safely print the string. If it is not printable, print its address
 * If the length of the string is too large, print '...'
 *
 * @param  fp The file stream to which the stack trace should
 *         be printed
 * @param  arg_val The value of the argument 
 * @return Void. 
 */
void print_string(FILE *fp, char *arg_val);

/**
 * @brief  Print the argument whose type is char**
 * 
 * Safely print the string array. If it is null, print its address
 * If the length of the array is too large, print '...'
 *
 * @param  fp The file stream to which the stack trace should
 *         be printed
 * @param  arg_val The value of the argument 
 * @return Void. 
 */
void print_string_array(FILE *fp, char **arg_val);

/**
 * @brief  Check if the string is printable
 *
 * Check each of the char in the string is printable
 * and it is printable only if all chars are printable  
 *         
 * @param  arg_val The address of the string to print
 * @param  length The length of the string to be got
 * @return 1 if the string is printable, 0 otherwise. 
 */
int is_string_print(char *arg_val, int *length);

/**
 * @brief  SIGSEGV_handler
 *
 * Jump to the place where we have already set, so whenever we detect  
 * jump happens from the return of the sigsetjmp() funtion, we can know 
 * that the address is invalid. Don't use write() function because it 
 * may not have privilige to write the file. Don't use mmap() becuase
 * it is too costly to just validate one address. The limitation of using
 * 
 * @param  sig The signal invoke this function
 * @return Void. 
 */
void SIGSEGV_handler(int sig);

/**
 * @brief  Setup the SIGSEGV handler
 *
 * @param  fp The file stream to print
 * @return Void. 
 */
void SIGSEGV_handler_setup(FILE *fp);

/* Global variable */
sigjmp_buf jump_to; // place to jump to when SIGSEGV happens

void traceback(FILE *fp)
{
    int *ebp, *old_ebp;
    int return_address = -1;
    int index = -1, exit_address = -1;
    int exit = -1;

    SIGSEGV_handler_setup(fp);

    functsym_t curr_function;

    ebp = (int *)trace_init_ebp();
    /* trace back until meet the _start function */
    while (1) {
        /* ebp = 0 indicates the _start function and
            whenever address is invalid, quit */
        if (sigsetjmp(jump_to, 1) || !ebp) break;

        /* extract basic information, return address stores
           just on the top of the ebp pointer */
        old_ebp = (int *)(*ebp);
        return_address = *(ebp + 1);
        /* if ebp >= old_ebp the stack frame must be wrong */
        if (old_ebp && ebp >= old_ebp) {
            fprintf(fp, "FATAL: Stack Wrong!\n");
            break;
        }

        index = get_index(return_address);
        if (index < 0) {
            fprintf(fp, "Function 0x%x(...), in\n", return_address);
        } else {
            curr_function = functions[index];
            /* get suspicious exit address */
            exit_address = return_address + *(int*)(return_address + 4) + 8;
            exit = get_index(exit_address);
            fprintf(fp, "Function %s(", curr_function.name);
            print_arguments(fp, curr_function, old_ebp);
            fprintf(fp, "), in\n");
        }

        /* continue to trace the next function */
        ebp = old_ebp;
    }
}

int get_index(int return_address) {
    int index = -1;

    /* go through the function list and find the function list index */
    while (++index < FUNCTS_MAX_NUM && strlen(functions[index].name) != 0) {
        if ((int)functions[index].addr > return_address) {
            break;
        }
    }

    /* check the return function is right */
    if (index >= 0 && return_address - (int)functions[index - 1].addr
        < MAX_FUNCTION_SIZE_BYTES) {
        return index - 1;
    }
    return -1;
}

void print_arguments(FILE *fp, functsym_t function, int* ebp) {
    int index = -1;
    int *arg_val;
    int int_size = sizeof(int);
    argsym_t arg;

    /* go through the arge list and print them */
    while (++index < ARGS_MAX_NUM && strlen(function.args[index].name) != 0) {
        arg = function.args[index];
        /* print ',' first when there are more than one argument */
        if (index > 0) {
            fprintf(fp, ", ");
        }

        /* get the real value's address of the argument */
        arg_val = (int*)(ebp + arg.offset / int_size);
        
        /* print the argument according to its type */
        switch (arg.type) {
        case TYPE_CHAR:
            if (isprint(*(char*)arg_val)) {
                fprintf(fp, "char %s='%c'", arg.name, *(char*)arg_val);
            } else {
                fprintf(fp, "char %s='\\%o'", arg.name, *(char*)arg_val);
            }
            break;
        case TYPE_INT:
            fprintf(fp, "int %s=%d", arg.name, *arg_val);
            break;
        case TYPE_FLOAT:
            fprintf(fp, "float %s=%f", arg.name, *(float*)arg_val);
            break;
        case TYPE_DOUBLE:
            fprintf(fp, "double %s=%lf", arg.name, *(double*)arg_val);
            break;
        case TYPE_STRING:
            fprintf(fp, "char *%s=", arg.name);
            print_string(fp, (char*)*arg_val);
            break;
        case TYPE_STRING_ARRAY:
            fprintf(fp, "char **%s=", arg.name);
            print_string_array(fp, (char**)*arg_val);
            break;
        case TYPE_VOIDSTAR:
            fprintf(fp, "void *%s=0v%x", arg.name, *arg_val);
            break;
        default:
            fprintf(fp, "UNKNOWN %s=%#x", arg.name, (int)arg_val);
        }
    }

    /* no argument, print 'void' */
    if (index == 0) {
        fprintf(fp, "void");
        return;
    }

}

void print_string(FILE *fp, char *arg_val) {
    int length = 0, i = 0;

    /* check the string is printable */
    if (is_string_print(arg_val, &length)) {
        fprintf(fp, "\"");
        for (i = 0; i < length && i < MAX_STRING_LEN; i++) {
            fprintf(fp, "%c", arg_val[i]);
        }

        /* print '...' if the length is too large */
        if (i >= MAX_STRING_LEN && i < length) {
            fprintf(fp, "...");
        }

        fprintf(fp, "\"");
    } else {
        fprintf(fp, "0x%x", (int)arg_val);
    }
}

void print_string_array(FILE *fp, char **arg_val) {
    int i;
    /* if address is not valid, print the address of the array */
    if (sigsetjmp(jump_to, 1)) {
        fprintf(fp, "%#x", (int)arg_val);
        return;
    }
    /* if NULL, the array is not printable */
    if (!arg_val) {
        fprintf(fp, "0x0");
        return;
    }
    /* check the string array is printable */
    for (i = 0; i < MAX_ARRAY_LEN; i++) {
        /* try the address of each string argument */
        if (!arg_val[i]) {
            break;
        }
    }

    i = 0;
    fprintf(fp, "{");
    while(arg_val[i] && i < MAX_ARRAY_LEN) {
        /* print ',' when it is not the first element*/
        if (i > 0) {
            fprintf(fp, ", ");
        }

        print_string(fp, arg_val[i]);
        i++;
    }

    /* if too much string in the array, print '...' */
    if (arg_val[i] && i >= MAX_ARRAY_LEN) {
        fprintf(fp, ", ...");
    }
    fprintf(fp, "}");
}

int is_string_print(char *arg_val, int *length) {
    int len = 0;

    /* judge if the address is valid */
    if (sigsetjmp(jump_to, 1) || !arg_val) {
        return 0;
    }

    /* go through the string to see whether it is printable */
    while(1) {
        /* while loop end indicator */
        if (arg_val[len] == '\0') {
            break;
        }

        /* one char is not printable, return 0 */
        if (!isprint(arg_val[len])) {
            return 0;
        }
        len++;
    }

    *length = len;
    return 1;
}

void SIGSEGV_handler_setup(FILE* fp) {
    sigset_t signal_set;
    struct sigaction sa;

    /* Set up the signal handler */
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIGSEGV_handler;
    sa.sa_flags = SA_SIGINFO;
    if (sigaction(SIGSEGV, &sa, NULL) < 0) {
        fprintf(fp, "Setting up handler failed: sigaction");
        exit(-1);
    }

    /* Unblock all signals */
    sigfillset(&signal_set);
    if (sigprocmask(SIG_UNBLOCK, &signal_set, NULL) < 0) {
        fprintf(fp, "Setting up handler failed: sigprocmask");
        exit(-1);
    }
}

void SIGSEGV_handler(int sig) {
    siglongjmp(jump_to, 1);
}

#ifndef _SH_H
#define _SH_H

#include <stdint.h>

#define CMD_LINE_LEN 128
#define MAX_ARG_NR 16

typedef int (*cmd_func_t) (int argc, char **argv);

/* buildin cmd struct */
struct buildin_cmd {
    char *name;
    cmd_func_t cmd_func;
};

//func
void print_prompt();
int cmd_parse(char * cmd_str, char **argv, char token);
void readline( char *buf, uint32_t count);
int execute_cmd(int argc, char **argv);
void sh_exit_trigger(int trigno);
void print_logo();

#endif /* _SH_H */
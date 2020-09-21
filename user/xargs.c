#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/param.h"
#include "user/user.h"

#define BUFSIZE 512

int fd[2];

int
main(int argc, char **argv)
{
  if (argc < 2) {
    fprintf(2, "Usage: xargs <program> <args>");
  }

  // get stdin
  int stdin_arg_idx = 0;
  char *stdin_args[MAXARG];
  char buf[BUFSIZE];
  while (1) {
    gets(buf, BUFSIZE);
    int len = strlen(buf)-1;
    if (len < 0)
      break;
    buf[len] = '\0'; // avoid new line in our args
    stdin_args[stdin_arg_idx] = malloc(strlen(buf)+1);
    memmove(stdin_args[stdin_arg_idx], buf, strlen(buf)+1);
    stdin_arg_idx++;
  }

  int pid = fork();
  if (pid == 0) { /* child */
    /* copy original args list and append stdin from parent as the rest of the args */
    /* copy original args list */
    char *child_argv[MAXARG];
    for (int i = 1; i < argc; ++i) {
      child_argv[i-1] = argv[i];
    }
    /* copy stdin args list */
    for (int i = 0; i < stdin_arg_idx; ++i) {
      child_argv[argc-1 + i] = stdin_args[i];
    }
    /* execute */ 
    exec(child_argv[0], &child_argv[0]);
  }
  else { /* parent */
    /* now wait for child */
    int xstatus;
    wait(&xstatus);
  }

  /* cleanup stdin args list */
  for (int i = argc-1; i < stdin_arg_idx; ++i) {
    free(stdin_args[i]);
  }

  exit(0);
}

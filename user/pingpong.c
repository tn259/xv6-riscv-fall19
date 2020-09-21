#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int fd[2];

void 
read_message()
{
  /* read */
  char buf[5];
  int count = read(fd[0], buf, sizeof(buf)-1);
  if (count <= 0) {
    fprintf(2, "read\n");
    exit(1);
  }
  buf[count] = '\0';
  fprintf(1, "%d: received %s\n", getpid(), buf);
}

void
write_message(const char* message)
{
  int length = strlen(message);
  write(fd[1], message, length);
}

int
main(int argc, char **argv)
{
  if (pipe(fd) == -1) {
    fprintf(2, "pipe\n");
    exit(1);
  }

  int pid = fork();
  if (pid == 0) { /* child */
    read_message();
    const char* message = "pong";
    write_message(message);
  }
  else { /* parent */
    const char* message = "ping";
    write_message(message);
    int xstatus;
    wait(&xstatus);
    read_message();
  }
  exit(0);
}

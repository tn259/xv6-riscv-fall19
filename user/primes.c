#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int fd[2];

void sieve(int oldfd) {
  int p, n, pid;
  while (read(oldfd, &p, sizeof(int)) == sizeof(int)) {
    fprintf(1, "prime %d\n", p);

    if (pipe(fd) == -1) { // just ignore and pass through
      //fprintf(2, "pipe\n");
    }

    pid = fork();
    if (pid == -1) { // just ignore and pass through
    }
    else if (pid == 0) { /* child */
      close(fd[1]);
      oldfd = fd[0];
      continue;  
    }
    else { /* parent */
      close(fd[0]);
      while (read(oldfd, &n, sizeof(int)) == sizeof(int)) {
        if (n % p != 0) {
	  write(fd[1], &n, sizeof(int));
	}
      }
      close(fd[1]);
      wait(0);
      break;
    }
  }
  close(oldfd);
  exit(0);
}

int
main(int argc, char **argv)
{
  if (pipe(fd) == -1) {
    fprintf(2, "pipe\n");
    exit(1);
  }

  int pid = fork();
  if (pid == -1) {
    fprintf(2, "1 fork\n");
    exit(1);
  }
  else if (pid == 0) { /* child */
    close(fd[1]);
    sieve(fd[0]);
    exit(0); 
  }
  else { /* parent */
    close(fd[0]);
    int n = 2;
    for (; n <= 35; ++n) {
      write(fd[1], &n, sizeof(int));
    }
    close(fd[1]);
    wait(0);
    exit(0);
  }
}

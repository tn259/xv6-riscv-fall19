// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

// Parsed command representation
#define MAXARGS 10
#define MAXPIPEDCMDS 10
#define MAXTOKENLEN 20

struct execcmd {
  int argc;
  char *argv[MAXARGS];
  char *endargv[MAXARGS];
  char infile[MAXTOKENLEN]; // NULL if n/a
  char *endinfile;
  char outfile[MAXTOKENLEN]; // NULL if n/a
  char *endoutfile;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
int parsecmd(char*, struct execcmd *cmds);

// Execute cmd.  Never returns.
void
runcmd(struct execcmd *cmd, int pipeidx, int numpipes)
{
  if (pipeidx == numpipes)
    exit(0);

  int p[2];

  if(cmd == 0)
    exit(1);

  int hasInfile = 0;
  int hasOutfile = 0;

  if (*cmd->infile != 0x0) {
    close(0);
    if (open(cmd->infile, O_RDONLY) < 0) {
      fprintf(2, "open %s failed\n", cmd->infile);
      exit(1);
    }
    hasInfile = 1;
  }

  if (*cmd->outfile != 0x0) {
    close(1);
    if (open(cmd->outfile, O_WRONLY|O_CREATE) < 0) {
      fprintf(2, "open %s failed\n", cmd->outfile);
      exit(1);
    }
    hasOutfile = 1;
  }

  if (hasInfile) {
    if (pipeidx > 0) {
      panic("Input redirection only allowed for first process in pipeline");
    }
  }

  if (pipeidx < numpipes-1) { // pipes left to create
    if (hasOutfile)
      panic("Output redirection only allowed for last process in pipeline");
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() != 0) { // parent
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      if (cmd->endargv[0] == 0)
        exit(1); // no command parsed!
      if (exec(cmd->argv[0], cmd->argv) != 0)
	panic("piped exec");
    }
    else { // child
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(++cmd, ++pipeidx, numpipes);
    }
    wait(0);
  }
  else { // no pipes left
    if (cmd->endargv[0] == 0)
      exit(1); // no command parsed!
    if (exec(cmd->argv[0], cmd->argv) != 0)
      panic("exec");
  }

  exit(0);
}

int
getcmd(char *buf, int nbuf)
{
  fprintf(2, "@ ");
  memset(buf, 0, nbuf);
  gets(buf, nbuf);
  if(buf[0] == 0) // EOF
    return -1;
  return 0;
}

int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' '){
      // Chdir must be called by the parent, not the child.
      buf[strlen(buf)-1] = 0;  // chop \n
      if(chdir(buf+3) < 0)
        fprintf(2, "cannot cd %s\n", buf+3);
      continue;
    }
    struct execcmd cmds[MAXPIPEDCMDS];
    if(fork1() == 0)
    {
      struct execcmd *startcmd = cmds;
      int numpipes = parsecmd(buf, cmds);
      runcmd(startcmd, 0, numpipes);
    }
    wait(0);
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s)) // chomp whitespace
    s++;
  if(q) // start ptr of token
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      fprintf(2, "Appending not supported\n");
      panic("gettoken");
    }
    break;
  default:
    ret = 'a'; // not symbol and not whitespace
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq) // end ptr of token
    *eq = s;

  while(s < es && strchr(whitespace, *s)) // chomp whitespace
    s++;
  *ps = s;
  return ret;
}

/* advaces current string of ps through whitespace
 * determines whether any of toks are contained in the string
 */
int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

int parseline(char**, char*, struct execcmd*);
void parsepipe(char**, char*, struct execcmd*, int*);
void parseexec(char**, char*, struct execcmd*);
void nulterminate(struct execcmd*, int numpipes);

int
parsecmd(char *s, struct execcmd *cmd)
{
  char *es;

  // set in and out file null if no redirection occurs
  *cmd->infile = 0x0;
  *cmd->outfile = 0x0;

  es = s + strlen(s);
  int numpipes = parseline(&s, es, cmd);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd, numpipes);
  return numpipes;
}

int
parseline(char **ps, char *es, struct execcmd *cmd)
{
  int pipecount = 0;
  parsepipe(ps, es, cmd, &pipecount);
  return pipecount;
}

void
parsepipe(char **ps, char *es, struct execcmd *cmd, int *pipecount)
{
  ++(*pipecount);
  if (*pipecount > MAXPIPEDCMDS)
    panic("too many pipe commands");

  parseexec(ps, es, cmd);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    parsepipe(ps, es, ++cmd, pipecount);
  }
}

void
parseredirs(char **ps, char *es, struct execcmd *cmd)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    if (eq-q >= MAXTOKENLEN)
      panic("filename length too long");
    switch(tok){
    case '<': 
      memmove(cmd->infile, q, eq-q);
      cmd->endinfile = eq;
      break;
    case '>':
      memmove(cmd->outfile, q, eq-q);
      cmd->endoutfile = eq;
      break;
    }
  }
}

void
setargv(char **argv, char **endargv, char *q, char *eq)
{
  *argv = q;
  *endargv = eq;
}

void
parseexec(char **ps, char *es, struct execcmd *cmd)
{
  char *q, *eq;
  int tok, argc;

  argc = 0;
  parseredirs(ps, es, cmd);
  while(!peek(ps, es, "|")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    //memmove(cmd->argv[argc], q, eq-q);
    setargv(&cmd->argv[argc], &cmd->endargv[argc], q, eq);
    cmd->endargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    parseredirs(ps, es, cmd);
  }
  cmd->argc = argc;
  *cmd->argv[argc] = 0;
  *cmd->endargv[argc] = 0;
}

// NUL-terminate all the counted strings.
void
nulterminate(struct execcmd *cmd, int numpipes)
{
  for (int pipedcmd = 0; pipedcmd < numpipes; ++pipedcmd) {
    // set all endargv to null
    for (int arg = 0; arg < cmd->argc; ++arg) {
      *cmd->endargv[arg] = 0x0;
    }

    // set endinfile and endoutfile to null
    *cmd->endinfile = 0x0;
    *cmd->endoutfile = 0x0;

    // advance cmd
    ++cmd;
  }
} 

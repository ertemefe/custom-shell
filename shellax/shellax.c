#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define READ_END 0
#define WRITE_END 1
#define BUFFERSIZE 128

const char *sysname = "shellax";
int m_flag = 0;
bool isFirst = true;

enum return_codes
{
  SUCCESS = 0,
  EXIT = 1,
  UNKNOWN = 2,
};

struct command_t
{
  char *name;
  bool background;
  bool auto_complete;
  int arg_count;
  char **args;
  char *redirects[3];     // in/out redirection
  struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command)
{
  int i = 0;
  printf("Command: <%s>\n", command->name);
  printf("\tIs Background: %s\n", command->background ? "yes" : "no");
  printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
  printf("\tRedirects:\n");
  for (i = 0; i < 3; i++)
    printf("\t\t%d: %s\n", i,
           command->redirects[i] ? command->redirects[i] : "N/A");
  printf("\tArguments (%d):\n", command->arg_count);
  for (i = 0; i < command->arg_count; ++i)
    printf("\t\tArg %d: %s\n", i, command->args[i]);
  if (command->next)
  {
    printf("\tPiped to:\n");
    print_command(command->next);
  }
}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
  if (command->arg_count)
  {
    for (int i = 0; i < command->arg_count; ++i)
      free(command->args[i]);
    free(command->args);
  }
  for (int i = 0; i < 3; ++i)
    if (command->redirects[i])
      free(command->redirects[i]);
  if (command->next)
  {
    free_command(command->next);
    command->next = NULL;
  }
  free(command->name);
  free(command);
  return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
  char cwd[1024], hostname[1024];
  gethostname(hostname, sizeof(hostname));
  getcwd(cwd, sizeof(cwd));
  printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
  return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
  const char *splitters = " \t"; // split at whitespace
  int index, len;
  len = strlen(buf);
  while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
  {
    buf++;
    len--;
  }
  while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
    buf[--len] = 0; // trim right whitespace

  if (len > 0 && buf[len - 1] == '?') // auto-complete
    command->auto_complete = true;
  if (len > 0 && buf[len - 1] == '&') // background
    command->background = true;

  char *pch = strtok(buf, splitters);
  if (pch == NULL)
  {
    command->name = (char *)malloc(1);
    command->name[0] = 0;
  }
  else
  {
    command->name = (char *)malloc(strlen(pch) + 1);
    strcpy(command->name, pch);
  }

  command->args = (char **)malloc(sizeof(char *));

  int redirect_index;
  int arg_index = 0;
  char temp_buf[1024], *arg;
  while (1)
  {
    // tokenize input on splitters
    pch = strtok(NULL, splitters);
    if (!pch)
      break;
    arg = temp_buf;
    strcpy(arg, pch);
    len = strlen(arg);

    if (len == 0)
      continue;                                          // empty arg, go for next
    while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
    {
      arg++;
      len--;
    }
    while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
      arg[--len] = 0; // trim right whitespace
    if (len == 0)
      continue; // empty arg, go for next

    // piping to another command
    if (strcmp(arg, "|") == 0)
    {
      struct command_t *c = malloc(sizeof(struct command_t));
      int l = strlen(pch);
      pch[l] = splitters[0]; // restore strtok termination
      index = 1;
      while (pch[index] == ' ' || pch[index] == '\t')
        index++; // skip whitespaces

      parse_command(pch + index, c);
      pch[l] = 0; // put back strtok termination
      command->next = c;
      continue;
    }

    // background process
    if (strcmp(arg, "&") == 0)
      continue; // handled before

    // handle input redirection
    redirect_index = -1;
    if (arg[0] == '<')
      redirect_index = 0;
    if (arg[0] == '>')
    {
      if (len > 1 && arg[1] == '>')
      {
        redirect_index = 2;
        arg++;
        len--;
      }
      else
        redirect_index = 1;
    }
    if (redirect_index != -1)
    {
      command->redirects[redirect_index] = malloc(len);
      strcpy(command->redirects[redirect_index], arg + 1);
      continue;
    }

    // normal arguments
    if (len > 2 &&
        ((arg[0] == '"' && arg[len - 1] == '"') ||
         (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
    {
      arg[--len] = 0;
      arg++;
    }
    command->args =
        (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
    command->args[arg_index] = (char *)malloc(len + 1);
    strcpy(command->args[arg_index++], arg);
  }
  command->arg_count = arg_index;

  command->args = (char **)realloc(command->args, sizeof(char *) *
                                                      (command->arg_count += 2));

  for (int i = command->arg_count - 2; i > 0; --i)
  {
    command->args[i] = command->args[i - 1];
  }
  command->args[0] = strdup(command->name);
  command->args[command->arg_count - 1] = NULL;

  return 0;
}

void prompt_backspace()
{
  putchar(8);   // go back 1
  putchar(' '); // write empty over
  putchar(8);   // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
  int index = 0;
  char c;
  char buf[4096];
  static char oldbuf[4096];

  // tcgetattr gets the parameters of the current terminal
  // STDIN_FILENO will tell tcgetattr that it should write the settings
  // of stdin to oldt
  static struct termios backup_termios, new_termios;
  tcgetattr(STDIN_FILENO, &backup_termios);
  new_termios = backup_termios;
  // ICANON normally takes care that one line at a time will be processed
  // that means it will return if it sees a "\n" or an EOF or an EOL
  new_termios.c_lflag &=
      ~(ICANON |
        ECHO); // Also disable automatic echo. We manually echo each char.
  // Those new settings will be set to STDIN
  // TCSANOW tells tcsetattr to change attributes immediately.
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

  show_prompt();
  buf[0] = 0;
  while (1)
  {
    c = getchar();
    // printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

    if (c == 9) // handle tab
    {
      buf[index++] = '?'; // autocomplete
      break;
    }

    if (c == 127) // handle backspace
    {
      if (index > 0)
      {
        prompt_backspace();
        index--;
      }
      continue;
    }

    if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68)
    {
      continue;
    }

    if (c == 65) // up arrow
    {
      while (index > 0)
      {
        prompt_backspace();
        index--;
      }

      char tmpbuf[4096];
      printf("%s", oldbuf);
      strcpy(tmpbuf, buf);
      strcpy(buf, oldbuf);
      strcpy(oldbuf, tmpbuf);
      index += strlen(buf);
      continue;
    }

    putchar(c); // echo the character
    buf[index++] = c;
    if (index >= sizeof(buf) - 1)
      break;
    if (c == '\n') // enter key
      break;
    if (c == 4) // Ctrl+D
      return EXIT;
  }
  if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
    index--;
  buf[index++] = '\0'; // null terminate string

  strcpy(oldbuf, buf);

  parse_command(buf, command);

  print_command(command); // DEBUG: uncomment for debugging

  // restore the old settings
  tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  return SUCCESS;
}

int process_command(struct command_t *command);

void pro_exec_with_pipe(struct command_t *command, int fd[]);
void uniq_with_pipe(struct command_t *command, bool isFirst);
void pro_exec(struct command_t *command);
void uniq(struct command_t *command);
void wiseman(struct command_t *command);
void news(struct command_t *command);
void weather(struct command_t *command);

int main()
{
  while (1)
  {
    struct command_t *command = malloc(sizeof(struct command_t));
    memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

    int code;
    code = prompt(command);
    if (code == EXIT)
      break;

    code = process_command(command);
    if (code == EXIT)
      break;

    free_command(command);
  }

  printf("\n");
  return 0;
}

int process_command(struct command_t *command)
{
  int r;
  if (strcmp(command->name, "") == 0)
    return SUCCESS;

  if (strcmp(command->name, "exit") == 0)
  {
    if (m_flag == 1)
    {
      system("sudo rmmod mymodule.ko");
      printf("Remove was succesfull\n");
    }
    return EXIT;
  }

  if (strcmp(command->name, "cd") == 0)
  {
    if (command->arg_count > 0)
    {
      r = chdir(command->args[0]);
      if (r == -1)
        printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
      return SUCCESS;
    }
  }

  if (strcmp(command->name, "uniq") == 0 && command->next == NULL)
  {
    // printf("uniq liycem normal\n");
    uniq(command);
    return SUCCESS;
  }

  // if (strcmp(command->name, "uniq") == 0 && command->next != NULL && isFirst == true)
  // {
  //     printf("uniq liycem normal\n");
  //     uniq(command);
  //     return SUCCESS;
  // }

  // BURASI
  pid_t pid = fork();
  if (pid == 0)
  {

    if (strcmp(command->name, "psvis") == 0)
    {
      psvis(command);
      exit(0);
    }
    if (strcmp(command->name, "wiseman") == 0)
    {
      wiseman(command);
      remove("wise.txt");
      exit(0);
    }
    if (strcmp(command->name, "weather") == 0)
    {
      weather(command);
      exit(0);
    }
    if (strcmp(command->name, "news") == 0)
    {
      news(command);
      exit(0);
    }

    if (command->next != NULL)
    {
      // printf("EXE\n");
      pro_exec_with_pipe(command, NULL);
    }
    else
    {
      pro_exec(command);
    }
    exit(0);
  }
  else
  {
    if (!(command->background))
    {
      printf("Not Background command !!\n");
      waitpid(pid, NULL, 0);
    }
    return SUCCESS;
  }

  printf("-%s: %s: command not found\n", sysname, command->name);
  return UNKNOWN;
}

const char *path_finder(char *name)
{
  // following code is inspired by https://codeforwin.org/2018/03/c-program-find-file-properties-using-stat-function.html for part 1(path finding)
  char PATH[256];
  strcpy(PATH, getenv("PATH"));
  char *token = strtok(PATH, ":");

  while (token != NULL)
  {

    struct stat *file_prop = malloc(sizeof(struct stat));
    char *command_path = malloc(256);

    strcat(command_path, token);
    strcat(command_path, "/");
    strcat(command_path, name);

    if (stat(command_path, file_prop) == 0)
    {
      return (command_path);
    }
    token = strtok(NULL, ":");
  }
  return "";
}

void pro_exec(struct command_t *command)
{

  // Part 2-A start
  if (command->redirects[0])
  {
    printf("User Entered : < \n");

    int file_desc = open(command->redirects[0], O_RDONLY, S_IRUSR | S_IWUSR);
    dup2(file_desc, 0);
    close(file_desc);
  }

  if (command->redirects[1])
  {
    printf("User Entered : > \n");

    int file_desc = open(command->redirects[1], O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
    dup2(file_desc, 1);
    close(file_desc);
  }

  if (command->redirects[2])
  {
    printf("User Entered : >> \n");
    int file_desc = open(command->redirects[2], O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    dup2(file_desc, 1);
    close(file_desc);
  }
  // Part 2-A end
  execv(path_finder(command->name), command->args);
}

void pro_exec_with_pipe(struct command_t *command, int fd[])
{
  // pids defined
  pid_t pid1;
  int fd_new[2];

  if (pipe(fd_new) == -1)
  {
    printf("Pipe failed\n");
    exit(0);
  }

  // First call to the function
  if (fd == NULL)
  {
    // printf("First command is handled \n");
    pid1 = fork();
    if (pid1 < 0)
    {
      printf("Pipe failed\n");
      return;
    }

    if (pid1 == 0)
    {
      if (!strcmp(command->name, "uniq"))
      {
        close(fd_new[READ_END]);
        dup2(fd_new[WRITE_END], STDOUT_FILENO);
        close(fd_new[WRITE_END]);
        uniq_with_pipe(command, true);
        exit(0);
      }
      else
      {
        close(fd_new[READ_END]);
        dup2(fd_new[WRITE_END], STDOUT_FILENO);
        close(fd_new[WRITE_END]);
        pro_exec(command);
      }
    }

    close(fd_new[WRITE_END]);
    waitpid(pid1, NULL, 0);
    pro_exec_with_pipe(command->next, fd_new);
  }
  else
  {
    // last command in the linked list
    if (command->next == NULL)
    {
      pid1 = fork();
      if (pid1 < 0)
      {
        printf("Pipe failed\n");
        return;
      }

      if (pid1 == 0)
      {
        if (!strcmp(command->name, "uniq"))
        {
          close(fd[WRITE_END]);
          close(fd_new[READ_END]);
          close(fd_new[WRITE_END]);

          dup2(fd[READ_END], STDIN_FILENO);
          close(fd[READ_END]);
          uniq_with_pipe(command, false);
          exit(0);
        }
        else
        {
          close(fd[WRITE_END]);
          close(fd_new[READ_END]);
          close(fd_new[WRITE_END]);

          // printf("Now executing last %s \n", command->name);
          dup2(fd[READ_END], STDIN_FILENO);
          close(fd[READ_END]);
          pro_exec(command);
        }
      }
    }
    else
    {
      pid1 = fork();
      if (pid1 < 0)
      {
        printf("Pipe failed\n");
        return;
      }

      if (pid1 == 0)
      {
        if (!strcmp(command->name, "uniq"))
        {
          close(fd[WRITE_END]);
          close(fd_new[READ_END]);

          dup2(fd[READ_END], STDIN_FILENO);
          close(fd[READ_END]);
          dup2(fd_new[WRITE_END], STDOUT_FILENO);
          close(fd_new[WRITE_END]);

          uniq_with_pipe(command, false);
          exit(0);
        }
        else
        {
          close(fd[WRITE_END]);
          close(fd_new[READ_END]);

          dup2(fd[READ_END], STDIN_FILENO);
          close(fd[READ_END]);
          dup2(fd_new[WRITE_END], STDOUT_FILENO);
          close(fd_new[WRITE_END]);

          pro_exec(command);
        }
      }
      waitpid(pid1, NULL, 0);
      close(fd_new[WRITE_END]);
      pro_exec_with_pipe(command->next, fd_new);
    }
  }

  // close pipe in the parent processes
  // waitpid(pid1, NULL, 0);
  if (fd != NULL)
  {
    close(fd[WRITE_END]);
    close(fd[READ_END]);
  }
  close(fd_new[WRITE_END]);
  close(fd_new[READ_END]);

  // wait for child processes
  waitpid(pid1, NULL, 0);
  // kill(pid1, SIGTERM);
}

void uniq_with_pipe(struct command_t *command, bool isFirst)
{
  char array[16][BUFFERSIZE];
  int index = 0;

  if (isFirst)
  {
    if (command->args[2] == NULL)
    {
      FILE *fp = fopen(command->args[1], "r");
      if (fp == NULL)
      {
        printf("cannot open the file! \n");
        exit(1);
      }

      // int fd_stdOut = open(STDOUT_FILENO,)
      while (fgets(array[index], BUFFERSIZE, fp) != NULL)
      {
        if (index > 0 && strcmp(array[index - 1], array[index]))
        {
          printf("%s", array[index - 1]);
        }
        index++;
      }
      fclose(fp);
    }
    else if (command->args[2] != NULL)
    {
      if ((!strcmp(command->args[1], "-c")) ||
          (!strcmp(command->args[1], "--count")))
      {
        // printf("flag active \n");

        FILE *fp = fopen(command->args[2], "r");
        if (fp == NULL)
        {
          printf("cannot open the file! \n");
          exit(1);
        }
        int count = 0;
        while (fgets(array[index], BUFFERSIZE, fp) != NULL)
        {
          if (index > 0 && strcmp(array[index - 1], array[index]))
          {
            // printf("%d %s", count, array[index - 1]);
            printf("%d %s", count, array[index - 1]);
            count = 1;
          }
          else
          {
            count++;
          }
          index++;
        }
        fclose(fp);
      }

      else if ((!strcmp(command->args[2], "-c")) ||
               (!strcmp(command->args[2], "--count")))
      {
        // printf("flag active \n");

        FILE *fp = fopen(command->args[1], "r");
        if (fp == NULL)
        {
          printf("cannot open the file! \n");
          exit(1);
        }
        int count = 0;
        while (fgets(array[index], BUFFERSIZE, fp) != NULL)
        {
          if (index > 0 && strcmp(array[index - 1], array[index]))
          {
            printf("%d %s", count, array[index - 1]);
            count = 1;
          }
          else
          {
            count++;
          }
          index++;
        }
        fclose(fp);
      }
      else
        printf("wrong flag \n");
    }
  }
  else
  {
    if (command->args[1] == NULL)
    {
      while (fgets(array[index], BUFFERSIZE, stdin) != NULL)
      {
        if (index > 0 && strcmp(array[index - 1], array[index]))
        {
          printf("%s", array[index - 1]);
        }
        index++;
      }
    }
    else if (command->args[1] != NULL)
    {
      if ((!strcmp(command->args[1], "-c")) ||
          (!strcmp(command->args[1], "--count")))
      {
        // printf("flag active \n");

        int count = 0;
        while (fgets(array[index], BUFFERSIZE, stdin) != NULL)
        {
          if (index > 0 && strcmp(array[index - 1], array[index]))
          {
            printf("%d %s", count, array[index - 1]);
            count = 1;
          }
          else
          {
            count++;
          }
          index++;
        }
      }
      else
        printf("wrong flag \n");
    }
  }
}

void uniq(struct command_t *command)
{
  char array[16][BUFFERSIZE];
  int index = 0;

  if (command->args[2] == NULL)
  {
    FILE *fp = fopen(command->args[1], "r");
    if (fp == NULL)
    {
      printf("cannot open file \n");
      exit(1);
    }

    while (fgets(array[index], BUFFERSIZE, fp) != NULL)
    {
      if (index > 0 && strcmp(array[index - 1], array[index]))
      {
        printf("%s", array[index - 1]);
        // fprintf(STDOUT_FILENO, "%s", array[index - 1]);
      }
      index++;
    }
    fclose(fp);
  }
  else if (command->args[2] != NULL)
  {
    if ((!strcmp(command->args[1], "-c")) ||
        (!strcmp(command->args[1], "--count")))
    {
      // printf("flag active \n");

      FILE *fp = fopen(command->args[2], "r");
      if (fp == NULL)
      {
        printf("cannot open the file! \n");
        exit(1);
      }
      int count = 0;
      while (fgets(array[index], BUFFERSIZE, fp) != NULL)
      {
        if (index > 0 && strcmp(array[index - 1], array[index]))
        {
          printf("%d %s", count, array[index - 1]);
          count = 1;
        }
        else
        {
          count++;
        }
        index++;
      }
      fclose(fp);
    }

    else if ((!strcmp(command->args[2], "-c")) ||
             (!strcmp(command->args[2], "--count")))
    {
      // printf("flag active \n");

      FILE *fp = fopen(command->args[1], "r");
      if (fp == NULL)
      {
        printf("cannot open the file! \n");
        exit(1);
      }
      int count = 0;
      while (fgets(array[index], BUFFERSIZE, fp) != NULL)
      {
        if (index > 0 && strcmp(array[index - 1], array[index]))
        {
          printf("%d %s", count, array[index - 1]);
          count = 1;
        }
        else
        {
          count++;
        }
        index++;
      }
      fclose(fp);
    }
    else
      printf("wrong flag \n");
  }
}

void wiseman(struct command_t *command)
{

  if (command->args[1] != NULL)
  {
    if (strcmp(command->args[1], "quit") == 0)
    {
      char *arg_cron[] = {"crontab", "-r", NULL};
      execv(path_finder("crontab"), arg_cron);
      printf("%s\n", "task removed");
    }
    else
    {
      char min[2];
      if (atoi(command->args[1]) < 60)
      {
        strcpy(min, command->args[1]);
      }
      else
      {
        printf("%s\n", "must be less than or equal to 59");
        return;
      }
      FILE *f;
      f = fopen("wise.txt", "w+");
      printf("%s\n", get_current_dir_name());
      fprintf(f, "*/%s * * * * %s > %s/fortune.txt\n", min,
              path_finder("fortune"), get_current_dir_name());
      fprintf(f, "*/%s * * * * %s -f %s/fortune.txt\n", min,
              path_finder("espeak"), get_current_dir_name());
      fclose(f);

      char *arg_cron[] = {"crontab", "wise.txt", NULL};
      execv(path_finder("crontab"), arg_cron);
    }
  }
  else
  {

    FILE *f;
    f = fopen("wise.txt", "w+");
    printf("%s\n", get_current_dir_name());
    fprintf(f, "*/1 * * * * %s > %s/fortune.txt\n",
            path_finder("fortune"), get_current_dir_name());
    fprintf(f, "*/1 * * * * %s -f %s/fortune.txt\n",
            path_finder("espeak"), get_current_dir_name());
    fclose(f);

    char *arg_cron[] = {"crontab", "wise.txt", NULL};
    execv(path_finder("crontab"), arg_cron);
  }
}

void news(struct command_t *command)
{
  system("sudo apt install python3-pip");
  system("pip install requests");
  system("pip install bs4");
  char call[BUFFERSIZE];
  strcpy(call, "python3 ");
  strcat(call, "./custom1.py ");
  system(call);
}

void weather(struct command_t *command)
{
  system("sudo apt install python3-pip");
  system("pip install requests");
  char call[BUFFERSIZE];
  strcpy(call, "python3 ");
  strcat(call, "./custom2.py ");
  strcat(call, command->args[1]);
  system(call);
}

void psvis(struct command_t *command)
{

  if (m_flag == 0)
  {
    char input[BUFFERSIZE];
    strcpy(input, "sudo insmod mymodule.ko PID=");
    strcat(input, command->args[1]);
    system("make");
    system(input);

    printf("Succesfully installed module\n");
    m_flag = 1;
  }
  else
  {
    printf("Module was intalled before\n");
  }

  system("dmesg > mesg.txt");
  system("tac mesg.txt | sed -n '/Loading Module/q;p' | awk '{print $2\" -> \" $3\";\"}' > msg.txt");
  system("{ echo 'digraph {'; tac msg.txt; echo '}'; } > newfile.txt");
  system("cat newfile.txt | dot -Tpng > output.png");

  remove("mesg.txt");
  remove("msg.txt");
  remove("newfile.txt");
  system("xdg-open output.png");
}
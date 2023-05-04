# COMP 304: Operating Systems Project 1: Shellax
#### Arda Aykan Poyraz - Efe Ertem
## Overview:
In the project we have implemented a custom shell/command line interface. The skeleton program is given as a starting point which contains the implementations for creating the environment and parsing of the Standard Input file for creating a link list command structs which contain the arguments of the command, a boolean for whether the process is a background process and a pointer to the next command if piping exists, if not is pointing to NULL.
NOTE: Before testing our implementation can prepend our project path to the PATH variable by : PATH=”OUR_PROJECT_DIRECTORY”+$PATH
## Part 1:
### execv:
In this part we have replaced the execvp() method with the execv method. Execvp is a built-in function that finds the path of the entered command and executes the command Execv takes the path as variable and executes the command in the given path. We came up with an abstract solution in which we created 2 methods: path_finder(char) and pro_exec(struct command_t *command). Path_finder finds the path of the given command’s name and returns the path. In the pro_exec, execv is used with the following form: execv(path_finder(command->name), command->args). This way execvp is replaced with a more dynamic fashion.
### background process(&):
Check the boolean for command->background if the boolean is true does not wait for the child processes which created to execute the command and if it is false wait for the child which is forked and executed in the if clause.
## Part 2:
### I/O redirections:
This part is injected into the pro_exec method. The struct command_t possesses a char array named redirects. During the pro_exec we check whether this command line entry has any redirections with this char array. Appropriate adjustments are made in these if statements with the dup2 function.
### Piping:
Piping is implemented recursively in pro_exec_with_pipe function. It accepts 2 arguments, first is the command which is a linked list and second is NULL for the first call to the function. The reason for that is to distinguish the first call to the function from others since the first call wont be reading from a pipe but directly executes the command with the given arguments. In other recursive calls a newly created pipe will be passed for the next command/child to read the stdout of the earlier calls. There is also another case which is the last argument it only reads and executes from the pipe that is passed but does not write to the newly created pipe. On all other cases which
## Part 3:
### Uniq:
It is called with “uniq” within the shellax. Our uniq command can handle both a std_in and a file.txt. If it is called with a pipe that passes a std_in the “uniq with pipe” function is called. If uniq is called with a format like “uniq file.txt'' the uniq function is called within the shellax program. Our uniq function can also handle the flags more dynamically, meaning it can handle the following commands: “uniq -c file.txt” and “uniq file.txt -c”. Its task is to output the uniq lines of an file. The flags “-c” or “--count” gives the uniq strings and their number of occurrences.
### Chatroom:
Chatroom gets two arguments first one is the roomName which is the name of the directory which pipes will be stored and the userName which is the name of the user's pipe. When each user executes the program they are welcomed with a “Welcome to RoomName !!” message. There are two child processes one for reading from the named pipes the other one is for writing to other users pipes but since there can be multiple users the child which is for writing has separate children for each user to write to their pipe when an input is received from the stdin. So, there are separate concurrent while loop for both writing and reading and actually there is a while loop for each other user so that concurrent writing is achieved. The user can exit the program by interrupting the execution.
### Wiseman:
It is called with “wiseman” within the shellax. In this task the shell creates a cronjob, which is a job that is repeated in a cycle. Crontab has a specific syntax (you can check it out in https://crontab.guru/). In the crontab job we had to schedule 2 jobs: creating a fortune and speaking that fortune through the speakers. For this job make sure that fortune and espeak is installed on your computer. wiseman can take in one argument, like “wiseman 5” which schedules the event to occur every 5th minute(*/5 * * * *). Or if it does not take in an argument it is automatically scheduled as every minute (*/1 * * * *).
### Custom Commands:
#### BBC News:
This custom command is made by Efe. when you enter “news” in the shellax, it outputs the names and the URL’s of the most read articles of the day on bbc news. This is written in python using the web scraping tool beautiful soup 4 and the requests library for python3.
#### Weather:
This custom command is made by Arda. When you enter “weather $city_name” in the shellax shell, it outputs the weather temperature, humidity, description, pressure etc. for the given city. It is written in python and uses the getweathermap.org API for the data. Then the JSON is processed within the python program and prints out the appropriate data. “requests” library for python3 is needed for this function.
## Part 4:
### Psvis:
It is called with “psvis”. Uses the system function to call the mymodule.ko file, following usage is appropriate: “psvis 1”. The second argument is passed on to the system function which internally calls the following “sudo insmod mymodule.ko PID=1”. We have implemented a kernel module programming file named mymodule.c. What that does is basically outputs all of the parent-child process “edges”. This output can be viewed when the “dmesg” function is called. What psvis does is that it takes the desired part of the output from the dmesg and then manipulates the string to create and open a PNG which shows all of the PIDs as a graph.

## Resources:
(https://www.tutorialspoint.com/c_standard_library/c_function_freopen.htm) <br />
(https://codeforwin.org/2018/03/c-program-find-file-properties-using-stat-function.html) <br />
(https://devarea.com/linux-kernel-development-and-writing-a-simple-kernel-module/#.Y3KfaOxByrM) <br />
(https://tldp.org/LDP/lkmpg/2.4/html/x354.htm) <br />
(https://www.informit.com/articles/article.aspx?p=368650) <br />

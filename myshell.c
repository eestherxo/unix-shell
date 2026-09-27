#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

int parse_input(char *buffer, char *args[], char **outfile)
{
	// input parsing
	char *token = strtok(buffer, " ");
	int i = 0;

	*outfile = NULL;

	while (token != NULL)
	{
		if (strcmp(token, ">") == 0)
		{
			// filename after >
			token = strtok(NULL, " ");

			if (token == NULL)
			{
				fprintf(stderr, "Missing expected filename after >\n");
				break;
			}
			*outfile = token;
		}
		else
		{
			args[i] = token;
			i++;
		}

		token = strtok(NULL, " ");
	}
	args[i] = NULL;
	return 0;
}

// manage PATH & external command execution
char *find_cmd(char *cmd)
{

	if (strchr(cmd, '/') != NULL)
		return cmd;

	char *path = getenv("PATH");
	if (path == NULL)
		return NULL;

	// to not modify the actual PATH
	char pathcopy[1024];
	strcpy(pathcopy, path);

	// tokenize PATH & build possible cmd path
	static char possible_path[1024];
	struct stat statbuffer;

	char *dir = strtok(pathcopy, ":");
	while (dir != NULL)
	{
		snprintf(possible_path, sizeof(possible_path), "%s/%s", dir, cmd);

		if (stat(possible_path, &statbuffer) == 0)
			return possible_path;

		dir = strtok(NULL, ":");
	}

	return NULL;
}

// shell loop
void shell_loop()
{

	char buffer[1024];
	char *args1[128];
	char *args2[128];
	char *outfile1 = NULL;
	char *outfile2 = NULL;

	while (1)
	{
		printf("[Enter Command]> ");
		fflush(stdout);

		if (fgets(buffer, sizeof(buffer), stdin) == NULL)
			break;

		// remove new line char from input buffer
		char *newline = strchr(buffer, '\n');
		if (newline != NULL)
			*newline = '\0';

		char *pipe_symbol = strchr(buffer, '|');
		if (pipe_symbol != NULL)
		{

			*pipe_symbol = '\0';

			char *left_cmd = buffer;
			char *right_cmd = pipe_symbol + 1;

			if (parse_input(left_cmd, args1, &outfile1) == -1 || parse_input(right_cmd, args2, &outfile2) == -1)
			{
				continue;
			}

			if (args1[0] == NULL || args2[0] == NULL)
			{
				fprintf(stderr, "Invalid pipe syntax\n");
				continue;
			}

			// pipe execution
			int pipe_file_descrips[2];
			if (pipe(pipe_file_descrips) == -1)
			{
				perror("pipe");
				continue;
			}

			pid_t left_pid = fork();
			if (left_pid == -1)
			{
				perror("fork");
				close(pipe_file_descrips[0]);
				close(pipe_file_descrips[1]);
				continue;
			}

			if (left_pid == 0)
			{
				if (dup2(pipe_file_descrips[1], STDOUT_FILENO) == -1)
				{
					perror("dup2");
					exit(1);
				}

				close(pipe_file_descrips[0]);
				close(pipe_file_descrips[1]);

				char *cmd_path = find_cmd(args1[0]);
				if (cmd_path == NULL)
				{
					fprintf(stderr, "%s: Command not found\n", args1[0]);
					exit(1);
				}

				execv(cmd_path, args1);
				perror("execv");
				exit(1);
			}

			pid_t right_pid = fork();
			if (right_pid == -1)
			{
				perror("fork");
				close(pipe_file_descrips[0]);
				close(pipe_file_descrips[1]);
				waitpid(left_pid, NULL, 0);
				continue;
			}

			if (right_pid == 0)
			{
				if (dup2(pipe_file_descrips[0], STDIN_FILENO) == -1)
				{
					perror("dup2");
					exit(1);
				}

				close(pipe_file_descrips[0]);
				close(pipe_file_descrips[1]);

				if (outfile2 != NULL)
				{
					int file_descrip = open(outfile2, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (file_descrip == -1)
					{
						perror("open");
						exit(1);
					}

					if (dup2(file_descrip, STDOUT_FILENO) == -1)
					{
						perror("dup2");
						close(file_descrip);
						exit(1);
					}

					close(file_descrip);
				}

				char *cmd_path = find_cmd(args2[0]);
				if (cmd_path == NULL)
				{
					fprintf(stderr, "%s: Command not found\n", args2[0]);
					exit(1);
				}

				execv(cmd_path, args2);
				perror("execv");
				exit(1);
			}

			close(pipe_file_descrips[0]);
			close(pipe_file_descrips[1]);
			waitpid(left_pid, NULL, 0);
			waitpid(right_pid, NULL, 0);
			continue;
		}
		else
		{

			if (parse_input(buffer, args1, &outfile1) == -1)
				continue;

			if (args1[0] == NULL)
				continue;
		}

		if (args1[0] != NULL && strcmp(args1[0], "exit") == 0)
			break;

		// prompt again if no cmd
		if (args1[0] == NULL)
			continue;

		char *cmd_path = find_cmd(args1[0]);
		if (cmd_path == NULL)
		{
			fprintf(stderr, "%s: Command not found\n", args1[0]);
			continue;
		}

		pid_t pid = fork();

		if (pid < 0)
			fprintf(stderr, "Fork Failed");
		else if (pid == 0)
		{

			// output redirection
			if (outfile1 != NULL)
			{

				int file_descrip = open(outfile1, O_WRONLY | O_CREAT | O_TRUNC, 0644);

				if (file_descrip == -1)
				{
					perror("open");
					exit(1);
				}

				if (dup2(file_descrip, STDOUT_FILENO) == -1)
				{
					perror("dup2");
					close(file_descrip);
					exit(1);
				}

				close(file_descrip);
			}
			execv(cmd_path, args1);
			perror("execv");
			exit(1);
		}
		else
		{
			int status;
			waitpid(pid, &status, 0);
		}
	}
}

int main(void)
{

	shell_loop();
	return 0;
}

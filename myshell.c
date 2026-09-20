#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

// output redirection & piping

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

	//  tokenize PATH & build possible cmd path
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
	char *args[128];

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

		// input parsing
		char *token = strtok(buffer, " ");
		int i = 0;
		while (token != NULL)
		{
			args[i] = token;
			i++;
			token = strtok(NULL, " ");
		}
		args[i] = NULL;

		if (args[0] != NULL && strcmp(args[0], "exit") == 0)
			break;

		// prompt again if no cmd
		if (args[0] == NULL)
			continue;

		char *cmd_path = find_cmd(args[0]);
		if (cmd_path == NULL)
		{
			fprintf(stderr, "%s: command not found\n", args[0]);
			continue;
		}

		pid_t pid = fork();

		if (pid < 0)
			fprintf(stderr, "Fork Failed");
		else if (pid == 0)
		{
			execv(cmd_path, args);
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

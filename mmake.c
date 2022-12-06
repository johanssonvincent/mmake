/**
 * @file mmake.c
 * @author Vincent Johansson (dv14vjn@cs.umu.se)
 * @brief
 * @version 0.1
 * @date 2022-10-19
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include "parser.h"

#define MAX_LINE 1024

typedef struct start_args
{
	int arg_b;
	int arg_s;
	int n_tar;
	int c_tar;
	int exitcode;
	char *makefile;
	char **target;
} start_args;

/* ---- Function declaration ---- */
void *init_struct(void);
void check_start_args(int argc, char *argv[], start_args *s);
makefile *choose_makefile(start_args *s);
void run_makefile(makefile *m, const char *target, start_args *s);
bool check_file(const char *current, const char *prereq, makefile *m);
void run_cmd(rule *tar_rule, start_args *s);
void *safe_calloc(size_t size);
void realloc_buff(char ***buffer, start_args *s);

int main(int argc, char *argv[])
{
	/* Initialize start_args struct */
	start_args *sa = init_struct();

	/* Check start arguments */
	check_start_args(argc, argv, sa);

	/* If flag -s is used, prohibit from printing messages */
	if (sa->arg_s == 1)
	{
		fclose(stdout);
	}

	/* If no targets specified, set target to default target */
	if (sa->c_tar == 0)
	{
		makefile *m = choose_makefile(sa);
		run_makefile(m, makefile_default_target(m), sa);
	}
	else
	{
		int i = 0;
		makefile *m = choose_makefile(sa);
		while (sa->target[i] != NULL)
		{
			run_makefile(m, sa->target[i], sa);
			i++;
		}
	}
	exit(sa->exitcode);
}

/* ---- Functions ---- */

/**
 * @brief Initialize start_args struct
 *
 * @return void*	pointer to allocated memory
 */
void *init_struct(void)
{
	/* Allocate memory for struct */
	start_args *sa;
	sa = safe_calloc(sizeof(start_args));

	/* Initialize values */
	sa->arg_b = 0;
	sa->arg_s = 0;
	sa->n_tar = 50;
	sa->c_tar = 0;
	sa->exitcode = 0;
	sa->makefile = NULL;

	/* Allocate memory for array where target names will be stored */
	sa->target = safe_calloc(sizeof(char *) * sa->n_tar);

	return sa;
}

/**
 * @brief Check start arguments given by user with getopt
 *
 * @param argc  	argc
 * @param argv		argv
 * @param s			start_args struct
 */
void check_start_args(int argc, char *argv[], start_args *s)
{
	int flag;

	while ((flag = getopt(argc, argv, ":Bsf:")) != -1)
	{
		switch (flag)
		{
		case 'f':
			s->makefile = optarg;
			break;
		case 'B':
			s->arg_b = 1;
			break;
		case 's':
			s->arg_s = 1;
			break;
		case '?':
			fprintf(stderr,
					"usage: ./mmake [-f MAKEFILE] [-B] [-s] [TARGET]\n");
			exit(errno);
		}
	}

	for (; optind < argc; optind++)
	{
		if (optind > s->n_tar)
		{
			realloc_buff(&s->target, s);
		}
		s->target[s->c_tar] = safe_calloc(sizeof(char) * MAX_LINE);
		strcpy(s->target[s->c_tar], argv[optind]);
		s->c_tar++;
	}
}

/**
 * @brief Choose makefile depending on userinput.
 * Either 'mmakefile' as default or file specified by user.
 *
 * @param s			start_args struct.
 * @return m     	The parsed makefile.
 */
makefile *choose_makefile(start_args *s)
{
	FILE *file;
	makefile *m;

	/*
	 * If no makefile has been specified through -f argument
	 * open default makefile (mmakefile), otherwise open
	 *  makefile specified by user.
	 */
	if (s->makefile == NULL)
	{
		if ((file = fopen("mmakefile", "r")) == NULL)
		{
			perror("mmakefile:");
			exit(errno);
		}

		if ((m = parse_makefile(file)) == NULL)
		{
			fprintf(stderr, "mmakefile: Could not parse makefile");
			exit(EXIT_FAILURE);
		}
		fclose(file);

		return m;
	}
	else
	{
		if ((file = fopen(s->makefile, "r")) == NULL)
		{
			fprintf(stderr, "%s:", s->makefile);
			perror("");
			exit(errno);
		}

		if ((m = parse_makefile(file)) == NULL)
		{
			fprintf(stderr, "%s: Could not parse makefile", s->makefile);
			exit(EXIT_FAILURE);
		}
		fclose(file);

		return m;
	}
}

/**
 * @brief Take parsed makefile and build from it.
 *
 * @param m 		the makefile
 * @param target	name of target
 * @param s			start_args struct
 */
void run_makefile(makefile *m, const char *target, start_args *s)
{
	rule *tar_rule;
	const char **tar_prereq;

	/* Get rule for target, if no rule exist return */
	if ((tar_rule = makefile_rule(m, target)) == NULL)
	{
		return;
	}

	/*
	 * Check prerequisites and recursively call
	 * run_makefile() to make them
	 */
	tar_prereq = rule_prereq(tar_rule);

	int i = 0;
	int j = 0;
	if (tar_prereq != NULL)
	{
		while (tar_prereq[i] != NULL)
		{
			run_makefile(m, tar_prereq[i], s);
			i++;
		}

		/* If option -B used, skip file check to force build */
		if (s->arg_b == 1)
		{
			run_cmd(tar_rule, s);
		}
		else
		{
			while (tar_prereq[j] != NULL)
			{
				if (check_file(target, tar_prereq[j], m))
				{
					run_cmd(tar_rule, s);
				}
				j++;
			}
		}
	}
	else
	{
		run_cmd(tar_rule, s);
	}
}

/**
 * @brief Check if files exist or needs to be created, if files exist
 * compare to see if prerequisite file was modified more recently than the
 * target. If there is no rule to make prerequisite, give error and exit.
 *
 * @param current	name of current target
 * @param prereq	name of prerequisite
 * @param m			the makefile
 * @return      	true or false
 */
bool check_file(const char *current, const char *prereq, makefile *m)
{
	/*
	 * Create structs for stat() func and variables for
	 * time value when files were edited
	 */
	struct stat stat_tar;
	struct stat stat_pre;
	time_t time_tar;
	time_t time_pre;

	/* Check if files exist, return true if a file needs to be created */
	if (access(prereq, F_OK) < 0)
	{
		if (makefile_rule(m, prereq) == NULL)
		{
			fprintf(stderr, "mmake: No rule to make target '%s'\n", prereq);
			exit(EXIT_FAILURE);
		}
		return true;
	}

	if (access(current, F_OK) < 0)
	{
		return true;
	}

	/* Collect information about files */
	lstat(prereq, &stat_pre);
	lstat(current, &stat_tar);

	/* Save time for last modification of files in time variables */
	time_tar = stat_tar.st_mtime;
	time_pre = stat_pre.st_mtime;

	/* Compare if prerequisite was modified after target */
	if (difftime(time_pre, time_tar) < 0 || difftime(time_pre, time_tar) == 0)
	{
		return false;
	}
	else
	{
		return true;
	}
}

/**
 * @brief Execute the command for given rule
 *
 * @param tar_rule	rule for target to be made
 * @param s			start_args struct
 */
void run_cmd(rule *tar_rule, start_args *s)
{
	pid_t pid;
	int i = 0;
	int status;

	/* Get command for the rule and print it */
	char **exec_cmd = rule_cmd(tar_rule);
	while (exec_cmd[i] != NULL)
	{
		if (exec_cmd[i + 1] == NULL)
		{
			printf("%s\n", exec_cmd[i]);
		}
		else
		{
			printf("%s ", exec_cmd[i]);
		}
		i++;
	}

	/* Fork to execute command */
	fflush(stdout);
	switch (pid = fork())
	{
	case -1:
		/* If fork failes, print error and exit */
		perror(strerror(errno));
		exit(errno);
	case 0: /* Child */
		/* Execute given command, if execvp fail print error and exit */
		if (execvp(exec_cmd[0], exec_cmd) < 0)
		{
			perror(strerror(errno));
			exit(errno);
		}
		break;
	default: /* Parent */
		/* Wait for child process to exit and save exit code */
		if (waitpid(pid, &status, 0) == -1)
		{
			perror(strerror(errno));
			exit(errno);
		}

		if (WIFEXITED(status))
		{
			s->exitcode = WEXITSTATUS(status);
		}
		break;
	}
}

/**
 * @brief Safe usage of malloc() function
 *
 * @param size      size of memory to be allocated
 * @return void*    pointer to allocated memory
 */
void *safe_calloc(size_t size)
{
	void *mem_block;

	if ((mem_block = calloc(1, size)) == NULL)
	{
		perror(strerror(errno));
		exit(errno);
	}

	return mem_block;
}

/**
 * @brief Reallocate **char buffer.
 *
 * @param buffer  	Pointer to **char buffer.
 * @param s			start_args struct.
 */
void realloc_buff(char ***buffer, start_args *s)
{
	char **temp;

	s->n_tar *= 2;

	temp = realloc(*buffer, sizeof(char *) * s->n_tar);
	if (temp == NULL)
	{
		perror("realloc()");
		exit(errno);
	}
	*buffer = temp;

	for (int i = 0; i < s->n_tar; i++)
	{
		char *temp2;
		temp2 = realloc(*buffer[i], sizeof(char) * MAX_LINE);
		if (temp2 == NULL)
		{
			perror("realloc()");
			exit(errno);
		}
		else
		{
			*buffer[i] = temp2;
		}
	}
}

#include "types.h"

#include "commands/cmd_create.h"
#include "commands/cmd_help.h"
#include "commands/cmd_insert.h"

#include <stdio.h>
#include <string.h>

static int exec_cmd(int (*op)(int,char**), int argc, char *argv[])
{
	if (op)
		return op(--argc, ++argv);
	
    	fprintf(stderr, "Unknown command: %s (type 'help' for commands)\n", argv[0]);
	return 1;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: %s <command>\n", argv[0]);
		return 1;
	}

	int (*op)(int, char**) = NULL;
	const char* const cmd = argv[1];
	if (!strcmp(cmd, "help"))
		op = &cmd_help;
	else if (!strcmp(cmd, "create"))
		op = &cmd_create;
	else if (!strcmp(cmd, "insert"))
		op = &cmd_insert;

	return exec_cmd(op, --argc, ++argv);
}

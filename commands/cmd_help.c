#include <stdio.h>

int cmd_help(int argc, [[maybe_unused]] char *argv[]) {
	puts(
		"Commands:\n"
		"  create table <name> \"(<col1>, <col2>, ...)\"\n"
		"  insert <name> <val1> <val2> ...\n"
		"  select <name>\n"
		"  help"
	);

	/* There isn't supposed to be any additional argument for 'help' */
	return argc ? 1 : 0;
}

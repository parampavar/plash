// Lists all available macros.

#define USAGE "usage: plash list-macros\n"

#include <unistd.h>

#include <utils.h>

int help_macros_main(int argc, char *argv[]) {
  execlp("plash", "plash", "eval", "--help", NULL);
  pl_fatal("execlp");
}

// usage: plash nodepath CONTAINER [--allow-root-container]
// Prints the path to a given container. The --allow-root-container option
// allows the root container ("0") to be specified as container.
//
// Example:
// $ plash nodepath 19
// /home/ihucos/.plashdata/layer/0/2/19
// $ plash nodepath 19 | xargs tree

#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <plash.h>

int main(int argc, char *argv[]) {

  int i = 0;
  char *nodepath, *plash_data;

  if (argc < 2)
    pl_usage();

  // validate/normalize input
  if (!argv[1][0] || strspn(argv[1], "0123456789") != strlen(argv[1]))
    pl_fatal("container arg must be a positive number, got: %s", argv[1]);

  if (0 == strcmp(argv[1], "0") &&
      (argc <= 2 || 0 != strcmp(argv[2], "--allow-root-container"))) {
    pl_fatal("container must not be the special root container ('0')");
  }

  plash_data = pl_call("data");
  if (chdir(plash_data) == -1 || chdir("index") == -1)
    pl_fatal("run `plash init`: chdir: %s", plash_data);

  if (!(nodepath = realpath(argv[1], NULL))) {
    errno = 0;
    pl_fatal("no container: %s", argv[1]);
  }
  puts(nodepath);
}

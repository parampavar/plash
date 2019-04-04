#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <unistd.h>

#include <plash.h>


#define CF_UNSHARE_USER      (1 << 0)
#define CF_COMMAND_NOT_FOUND (1 << 1)
#define CF_UNSHARE_MOUNT     (1 << 2)
#define CF_BUILD             (1 << 3)


struct cmdconf_entry {
        char *name;
        int flags;
};


void D(char *arr[]){
        int ai;
        for(ai=0; arr[ai]; ai++) fprintf(stderr, "%s, ", arr[ai]);
        fprintf(stderr, "\n");
}

struct cmdconf_entry all_cmdconfs[] = {
        {"add-layer",     CF_BUILD | CF_UNSHARE_USER},
        {"build",         },
        {"clean",         CF_UNSHARE_USER},
        {"copy",          CF_BUILD | CF_UNSHARE_USER},
        {"create",        CF_BUILD},
        {"data",          },
        {"eval",          },
        {"export-tar",    CF_BUILD | CF_UNSHARE_USER},

        {"help",          },
        {"-h",            },
        {"--help",        },

        {"help-macros",   },
        {"--help-macros", },

        {"import-docker", },
        {"import-lxc",    },
        {"import-tar",    },
        {"import-url",    },
        {"init",          },
        {"map",           },
        {"mount",         CF_BUILD},
        {"nodepath",      CF_BUILD},
        {"parent",        },
        {"purge",         CF_UNSHARE_USER},
        {"rm",            CF_BUILD | CF_UNSHARE_USER},
        {"run",           CF_BUILD},
        {"runopts",       CF_UNSHARE_USER | CF_UNSHARE_MOUNT},
        {"shallow-copy",  CF_BUILD | CF_UNSHARE_USER | CF_UNSHARE_MOUNT},
        {"shrink",        CF_UNSHARE_USER},
        {"sudo",          CF_UNSHARE_USER | CF_UNSHARE_MOUNT},
        {"test",          },

        {"version",       },
        {"--version",     },

        {"with-mount",    CF_BUILD | CF_UNSHARE_USER | CF_UNSHARE_MOUNT},
        {NULL, CF_COMMAND_NOT_FOUND},

};

int is_cli_param(char *param){
        switch(strlen(param)){
                case 1: return 0;
                case 2: return param[0] == '-' && param[1] != '-';
                default: return param[0] == '-';
        }
}


int get_cmd_flags(char *cmd){
        struct cmdconf_entry *currcmd;
        for(
            currcmd = all_cmdconfs;
            currcmd->name && strcmp(currcmd->name, cmd) != 0;
            currcmd++
        );
        return currcmd->flags;
}

void reexec_insert_run(int argc, char **argv){
        //  it: plash -A xeyes -- xeyes
        // out: plash run -A xeyes -- xeyes

        char *newargv_array[argc + 2];
        char **newargv = newargv_array;

        *(newargv++) = *(argv++);
        *(newargv++) = "run";
        while(*(newargv++) = *(argv++));

        execvp(newargv_array[0], newargv_array);
        pl_fatal("execvp");
}


void reexec_consume_build_args(int argc, char *argv[]){
        //  in: plash run -A xeyes -- xeyes
        // out: plash run 42 xeyes

        char  *build_array[argc],
             **build = build_array,
             **new_argv = argv,
             **orig_argv = argv;

        // "new_argv" and "build" get  argv[0] as first element
        *new_argv++ = *build++ = *argv++;

        // variable "build" has string "build" as second element
        *build++ = "build";

        // new_argv gets argv[1] as second element
        *new_argv++ = *argv++;

        // wind up all to the "build" variable until the end or "--" is
        // reached
        while((*argv && strcmp(*argv, "--") != 0))
                *build++ = *argv++;

        // chop "--" from our buffer, if any
        if (*argv) argv++;

        // "build" is done
        *build++ = NULL;

        // new_argv's third element is a freshly builded container id
        *new_argv++ = pl_check_output(build_array);

        // wind up all the rest to new_argv
        while(*argv) *new_argv++ = *argv++;
        *new_argv++ = NULL;

        execvp(orig_argv[0], orig_argv);
        pl_fatal("execvp");
}


int main(int argc, char* argv[]) {
        int flags;

        if (argc <= 1){
                fprintf(stderr, "plash is a container build and run engine, try --help\n");
                return 1;
        }

        //
        // load subcommand flags and handle implicit run
        //
        flags = get_cmd_flags(argv[1]);
        if (flags & CF_COMMAND_NOT_FOUND){
                if (is_cli_param(argv[1]))
                        reexec_insert_run(argc, argv);
                pl_fatal("no such command: %s (try `plash help`)", argv[1]);
        }

        //
        // handle build arguments
        //
        if (argc > 2 && flags & CF_BUILD && is_cli_param(argv[2]))
                reexec_consume_build_args(argc, argv);

        //
        // pop any "--" as first argument
        //
        if(argv[2] && strcmp(argv[2], "--") == 0){
                char *cmd = argv[1];
                argv[1] = argv[0];
                argv[2] = cmd;
                argv++;
        }

        struct passwd *pwd;
        char *bindir =               pl_path("../bin"),
             *libexecdir =           pl_path("../lib/exec"),
             *libexecrun =           pl_path("../lib/exec/run"),
             *pylibdir =             pl_path("../lib/py"),
             *path_env =             getenv("PATH"),
             *home_env =             getenv("HOME"),
             *plash_data_env =       getenv("PLASH_DATA"),
             *plash_no_unshare_env = getenv("PLASH_NO_UNSHARE"),
             *libexecfile,
             *newpath;

        //
        // setup environment variables
        //
        if (asprintf(&newpath, "%s:%s", bindir, path_env) == -1)
                pl_fatal("asprintf");
        if (setenv("PYTHONPATH", pylibdir , 1) == -1)
                pl_fatal("setenv");
        if (setenv("PATH", path_env ? newpath : bindir, 1) == -1)
                 pl_fatal("setenv");
        if (! plash_data_env){
            if (! home_env){
                pwd = getpwuid(getuid());
                if (! pwd)
                    pl_fatal("could not determine your home directory");
                home_env = pwd->pw_dir;
            }
            if (asprintf(&home_env, "%s/.plashdata", home_env) == -1)
                    pl_fatal("asprintf");
            if (setenv("PLASH_DATA", home_env , 1) == -1)
                    pl_fatal("setenv");
        }

        //
        // setup unsharing
        //
        if (!plash_no_unshare_env || plash_no_unshare_env[0] == '\0'){

                    if (flags & CF_UNSHARE_USER && getuid())
                        pl_setup_user_ns();

                    if (flags & CF_UNSHARE_MOUNT){
                            if (unshare(CLONE_NEWNS) == -1)
                                pl_fatal("could not unshare mount namespace");
                            if (mount("none", "/", NULL, MS_REC|MS_PRIVATE, NULL) == -1){
                                    if (errno != EINVAL)
                                        pl_fatal("could not change propagation of /");
                                    errno = 0;
                            }
                    }
        }

        //
        // exec lib/exec/<command>
        //
        if (asprintf(&libexecfile, "%s/%s", libexecdir, argv[1]) == -1)
                pl_fatal("asprintf");
        execvp(libexecfile, argv + 1);
        pl_fatal("could not exec %s", libexecfile);
}

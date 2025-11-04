#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <ipc_core.h>

static s32 ipc_tty_redirect(void)
{
    s32 tty  = -1;
    pv8 name = NULL;

    name = ttyname(STDOUT_FILENO);
    if (name == NULL) {
        printf("Error, get tty name failed\n");
        return IPC_FAILED;
    }

    tty = open(name, O_RDONLY | O_WRONLY);
    if (tty < 0) {
        printf("Error, open tty: %s failed\n", name);
        close(tty);
        return IPC_OPEN_ERROR;
    }

    s32 ret = ioctl(tty, TIOCCONS);
    if (ret < 0) {
        printf("ioctl TIOCCONS failed\n");
        close(tty);
        return IPC_IOCTL_ERROR;
    }

    printf("Successful redirection to %s\n", name);

    close(tty);

    return IPC_SUCCESS;
}

static void _show_modules(pv8 name)
{
    pcv8 level_map[] = {
        [IPC_LOG_NONE] = "none", [IPC_LOG_FATAL] = "fatal", [IPC_LOG_ERROR] = "error", [IPC_LOG_WARN] = "warn",
        [IPC_LOG_INFO] = "info", [IPC_LOG_DEBUG] = "debug", [IPC_LOG_TRACE] = "trace",
    };

    ipc_log_info_p p_info = NULL;
    while ((p_info = ipc_log_iter(p_info))) {
        if (name && strcmp(name, p_info->name))
            continue;
        printf("[%-5s] %-8s -> %s\n", level_map[p_info->level[0]], p_info->name, p_info->desc);
    }
}

static void _show_help()
{
    printf(
        "[Module] [Operation] :\n"
        "-t, --trace  : Verbose trace information\n"
        "-d, --debug  : Development debug flow information\n"
        "-i, --info   : Module-level status information\n"
        "-w, --warn   : Undesirable warning information\n"
        "-e, --error  : Runtime logic error information\n"
        "-f, --fatal  : System-level fatal information, which may cause device crashes\n"
        "-n, --none   : Disable all information\n"
        "-c, --coerce : Force set the specified log module\n"
        "==========================================\n");

    _show_modules(NULL);

    printf(
        "\n**********\nSpecial Operations\n\tlog redirect\n\tRedirect system stdout/stderr to the current "
        "terminal\n\n");
}

s32 main(s32 argc, pcv8 argv[])
{
    if (argc < 2) {
        _show_help();
        return IPC_INVALID_ARGS;
    }

    argc--;
    argv++;
    pv8 name = NULL;

    if (0 == strncmp(argv[0], "redirect", 8)) {
        ipc_tty_redirect();
        return IPC_SUCCESS;
    }

    for (s32 idx = 0; idx < argc; idx++) {
        if (argv[idx][0] != '-') {
            name = (pv8)argv[idx];
            break;
        }
    }

    ipc_opt_attr_t attr[] = {
        { 't', "trace", 1 }, { 'd', "debug", 1 }, { 'i', "info", 1 }, { 'w', "warn", 1 },
        { 'e', "error", 1 }, { 'f', "fatal", 1 }, { 'n', "none", 1 }, { 'c', "coerce", 1 },
    };

#define ERR_LEVEL 0xff
    u8 level  = ERR_LEVEL;
    u8 coerce = 0;
    ITER_INIT(iter, 1);
    s32 idx = ARRSIZE(attr);
    while (ipc_getopt_iter(iter[0], argc, argv, attr, &idx)) {
        switch (attr[idx].tag) {
            case 't':
                level = IPC_LOG_TRACE;
                break;
            case 'd':
                level = IPC_LOG_DEBUG;
                break;
            case 'i':
                level = IPC_LOG_INFO;
                break;
            case 'w':
                level = IPC_LOG_WARN;
                break;
            case 'e':
                level = IPC_LOG_ERROR;
                break;
            case 'f':
                level = IPC_LOG_FATAL;
                break;
            case 'n':
                level = IPC_LOG_NONE;
                break;
            case 'c':
                coerce = 1;
                break;
        }
    }

    if (level == ERR_LEVEL) {
        printf("Error : Incorrect argument input\n");
        _show_help();
        return IPC_INVALID_ARGS;
    }

    if (coerce)
        ipc_log_init(__IPC_LOG__, NULL, name, level, "");

    s32 ret = ipc_log_ctrl(0, name, level);
    if (ret == IPC_NOT_FOUND) {
        printf("Error : Module |-> %s <-| not found\n", name);
        _show_help();
        return ret;
    }

    _show_modules(name);

    return IPC_SUCCESS;
}
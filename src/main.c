#if defined(_MSC_VER) || defined(_WIN32) || defined(_WIN64) || defined(_WIN32_WCE)
#define _WIN_PLATFORM
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#ifndef _WIN_PLATFORM 
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#else
#include <windows.h>
#include <direct.h>
#endif

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#ifndef PATH_MAX
#ifdef _MAX_PATH
#define PATH_MAX _MAX_PATH
#else
#define PATH_MAX 260
#endif
#endif

#ifndef _WIN_PLATFORM
#define put_env(env_k, env_v) setenv(env_k, env_v, 1)
#else
#define put_env(env_k, env_v) _putenv_s(env_k, env_v, 1)
#define chdir _chdir 
#endif


static FILE *logger = NULL;

static void error_exit(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(logger, format, ap);
    va_end(ap);
    fflush(stdout);
    exit(1);
}

static bool init_debuglog() {
    if (!logger) {
        logger = freopen("debug.log", "w", stderr);
        if (logger == NULL)
            return false;
        setbuf(logger, NULL);
    }
    return true;
}

static void debuglog(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(logger, format, ap);
    va_end(ap);
}

static void change_workdir(const char *exe_path) {
#ifndef _WIN_PLATFORM 
    const char *pos = strrchr(exe_path, '/');
#else
    const char *pos = strrchr(exe_path, '\\');
#endif
    if (pos) {
        char workdir[PATH_MAX*2 + 1] = { 0 };
        int dirlen = pos - exe_path;
        strncpy(workdir, exe_path, dirlen);
        workdir[dirlen] = '\0';
        chdir(workdir);
    }
}

static void init_lua_path(lua_State *dL) {
    lua_getglobal(dL, "package");    // [pkg]
    lua_getfield(dL, -1, "path");    // [pkg|path]
    lua_pushstring(dL, "path");      // [pkg|path|pathkey]
    lua_pushfstring(dL, "../?.lua;../?.luac;%s", lua_tostring(dL, -2)); // [pkg|path|pathkey|pathval]
    lua_settable(dL, -4);    // [pkg|path]
    lua_pop(dL, 1); // [pkg]

    lua_getfield(dL, -1, "cpath");    // [pkg|cpath]
    lua_pushstring(dL, "cpath");     // [pkg|cpath|cpathkey]
    lua_pushfstring(dL, "./?.so;./?.dll;%s", lua_tostring(dL, -2)); // [pkg|cpath|cpathkey|cpathval]
    lua_settable(dL, -4);    // [pkg|path]
    lua_pop(dL, 2); // []
}

static bool run_script(lua_State *L) {
    if (luaL_loadfile(L, "../debugger.lua") == LUA_OK) {
#ifdef _WIN_PLATFORM
        lua_pushstring(L, "windows");
#elif defined(__APPLE__)
        lua_pushstring(L, "osx");
#else
        lua_pushstring(L, "linux");
#endif
        if (lua_pcall(L, 1, -1, 0) == LUA_OK) {
            return true;
        }
    }
    debuglog("%s\n", lua_tostring(L, -1));
    return false;
}

#ifndef _WIN_PLATFORM
static void run_skynet(const char *skynet, const char *config) {
    execl(skynet, skynet, config, NULL);
    error_exit("execl error: %s\n", strerror(errno));
}

static void spawn_child(const char *skynet, const char *config) {
    int pid = fork();
    if (pid == -1) {
        error_exit("fork error: %s\n", strerror(errno));
    } else if (pid != 0) {
        int state;
        if (wait(&state) == -1)
            error_exit("wait error: %s\n", strerror(errno));
        debuglog("child exit: %d\n", state);
    } else {
		debuglog("run_skynet pid: %d\n", getpid());
        run_skynet(skynet, config);
    }
}
#else
static void spawn_child(const char *skynet, const char *config) {
    STARTUPINFOW startup;
    PROCESS_INFORMATION info;
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    char cmdline[PATH_MAX*2 + 1] = { 0 };
    sprintf(cmdline, "%s %s", skynet, config);
    if (!CreateProcessA(NULL,
                        cmdline,
                        NULL,
                        NULL,
                        1,
                        0,
                        NULL,
                        NULL,
                        &startup,
                        &info)) {
        /* CreateProcess failed. */
        error_exit("create process error: %d\n", GetLastError()); 
    }
    debuglog("run_skynet pid: %d\n", info.dwProcessId);
    WaitForSingleObject(info.hProcess, INFINITE);
    CloseHandle(info.hProcess);
    CloseHandle(info.hThread);
    debuglog("child exit\n");
}
#endif

int main(int argc, char const *argv[]) {
#ifndef _WIN_PLATFORM
    signal(SIGHUP, SIG_IGN);
#endif
    if (argc > 0)
        change_workdir(argv[0]);
    if (!init_debuglog())
        error_exit("debug log failed");

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    init_lua_path(L);
    if (!run_script(L)) {
        error_exit("script error\n");
    }

    //workdir, skynet, config, service, open_debug, breakpoints, envs
    const char *workdir = lua_tostring(L, 1);
	const char *skynet = lua_tostring(L, 2);
    const char *config = lua_tostring(L, 3);
	const char *service = lua_tostring(L, 4);
    bool debug = lua_toboolean(L, 5);
    const char *breakpoints = lua_tostring(L, 6);

    put_env("vscdbg_open", debug ? "on" : "off");
    put_env("vscdbg_workdir", workdir);
    put_env("vscdbg_bps", breakpoints);
	put_env("vscdbg_service", service);
	debuglog("workdir: %s\n", workdir);
	debuglog("skynet path: %s\n", skynet);
	debuglog("config path: %s\n", config);
	debuglog("service path: %s\n", service);

    const char *env_k, *env_v;
    int env_sz = luaL_len(L, 7);
    for (int i = 1; i < env_sz;) {
        lua_rawgeti(L, 7, i++);
        lua_rawgeti(L, 7, i++);
        env_k = lua_tostring(L, -2);
        env_v = lua_tostring(L, -1);
        put_env(env_k, env_v);
        debuglog("user env: %s=%s\n", env_k, env_v);
        lua_pop(L, 2);
    }

    if (chdir(workdir) != 0) {
		error_exit("chdir failed: %s\n", strerror(errno));
	}

    spawn_child(skynet, config);

    lua_close(L);

    return 0;  
}

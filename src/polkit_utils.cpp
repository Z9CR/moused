#include <polkit_utils.hpp>
#include <utils.hpp>
// avoid errors on win
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <stdio.h>
#include <fstream>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>

/// find absolute path of current executable
/// on Linux uses /proc/self/exe, on BSDs uses realpath(argv[0])
/// unix & unix-likes only!
static const char *get_exe_path(const char *argv0)
{
    static char path[PATH_MAX];

#if defined(__linux__)
    // Linux: /proc/self/exe is always reliable
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1)
    {
        path[len] = '\0';
        return path;
    }
#endif
    // BSDs / fallback: resolve argv[0]
    if (realpath(argv0, path))
        return path;

    return argv0; // last resort
}

// the program must have privilege in Linux and BSDs to RW /dev/uinput(Linux) or /dev/wsmouse(BSDs)
void polkit_root_getter(int argc, char* argv[])
{
    if (geteuid() != 0)
    {
        // We are a normal user. Save the display environment and then
        // re-execute ourselves via pkexec so we can open /dev/uinput.
        const char *me = get_exe_path(argc > 0 ? argv[0] : "moused");

        // Choose a private temp path
        char envfile[256];
        snprintf(envfile, sizeof(envfile), "/tmp/moused_env_%d", getuid());

        // Save original uid and critical display environment variables
        std::ofstream f(envfile);
        if (f)
        {
            f << "MU_ORIG_UID=" << getuid() << '\n';

            const char *vars[] = {"DISPLAY", "WAYLAND_DISPLAY", "XAUTHORITY",
                                  "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS",
                                  "HOME", nullptr};
            for (const char **vp = vars; *vp; ++vp)
            {
                const char *val = getenv(*vp);
                if (val)
                    f << *vp << '=' << val << '\n';
            }
        }

        // Restrict permissions so only this user (and root) can read it
        chmod(envfile, 0600);

        execlp("pkexec", "pkexec", me, "--restore-env", (char *)NULL);

        // pkexec failed (not installed / user cancelled / no desktop session)
        // fallback: suggest manual sudo/doas
        unlink(envfile);
        log_msg("moused: root required (need write access to /dev/uinput).\n");
        log_msg("  polkit elevation failed — run with: sudo moused\n");
        exit(1);
    }

    // We are root (launched via pkexec or sudo).
    // Look for saved environment from the original user.
    {
        // Try each possible uid (we don't know which user started us).
        // Use the envfile from the SUDO_UID / PKEXEC_UID if available,
        // otherwise scan common uids on a desktop machine.
        uid_t orig_uid = (uid_t)-1;
        const char *sudo_uid = getenv("SUDO_UID");
        const char *pkexec_uid = getenv("PKEXEC_UID");
        if (sudo_uid) orig_uid = (uid_t)atoi(sudo_uid);
        else if (pkexec_uid) orig_uid = (uid_t)atoi(pkexec_uid);

        char envfile[256];
        if (orig_uid != (uid_t)-1)
        {
            snprintf(envfile, sizeof(envfile), "/tmp/moused_env_%u", orig_uid);
        }
        else
        {
            // Fallback: scan /tmp for moused_env_* files
            // For simplicity, try the most likely uid (1000 is common first desktop user)
            snprintf(envfile, sizeof(envfile), "/tmp/moused_env_1000");
        }

        std::ifstream f(envfile);
        if (f)
        {
            char line[1024];
            while (f.getline(line, sizeof(line)))
            {
                // getline() strips the trailing newline
                char *eq = strchr(line, '=');
                if (eq)
                {
                    *eq = '\0';
                    const char *key = line;
                    const char *val = eq + 1;

                    if (strcmp(key, "MU_ORIG_UID") == 0)
                    {
                        orig_uid = (uid_t)atoi(val);
                    }
                    else
                    {
                        // Restore environment variable (overwrite pkexec's root env
                        // with the original user's values)
                        setenv(key, val, 1);
                    }
                }
            }
            unlink(envfile);

            // HOME is now saved & restored via the envfile mechanism above
        }
    }

    // Now we are still root.  The uinput fd will be opened by
    // platform_uinput_setup() (called from main after this returns).
    // After that we will drop privileges.

    (void)argc;
    (void)argv;
}

/// Drop root privileges back to the original user.
/// Call this after platform_uinput_setup() has succeeded.
void polkit_drop_privileges()
{
    uid_t orig_uid = (uid_t)-1;
    const char *sudo_uid = getenv("SUDO_UID");
    const char *pkexec_uid = getenv("PKEXEC_UID");
    if (sudo_uid) orig_uid = (uid_t)atoi(sudo_uid);
    else if (pkexec_uid) orig_uid = (uid_t)atoi(pkexec_uid);

    if (orig_uid == (uid_t)-1 || orig_uid == 0)
    {
        // We couldn't determine the original user, or it was already root.
        // Stay root – this is fine if the user ran via sudo from a terminal.
        return;
    }

    // Drop effective uid (keep permitted set in case we need to regain).
    // seteuid changes only the effective uid so the process still has
    // CAP_SETUID in its permitted set and could regain root if needed.
    if (seteuid(orig_uid) != 0)
    {
        log_msg("moused: warning: seteuid(%d) failed: %s\n",
                orig_uid, strerror(errno));
    }
}
#endif
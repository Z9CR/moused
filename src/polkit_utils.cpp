#include <polkit_utils.hpp>
#include <utils.hpp>
// avoid errors on win
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#include <stdio.h>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>

/// find absolute path of current executable
/// on Linux uses /proc/self/exe, on BSDs uses realpath(argv[0])
/// unix & unix-likes only!
static std::string get_exe_path(const char *argv0)
{
    char path[PATH_MAX];

#if defined(__linux__)
    // Linux: /proc/self/exe is always reliable
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1)
    {
        path[len] = '\0';
        return std::string(path);
    }
#endif
    // BSDs / fallback: resolve argv[0]
    if (realpath(argv0, path))
        return std::string(path);

    return std::string(argv0); // last resort
}

// the program must have privilege in Linux and BSDs to RW /dev/uinput(Linux) or /dev/wsmouse(BSDs)
void polkit_root_getter(int argc, char* argv[])
{
    if (geteuid() != 0)
    {
        // We are a normal user. Save the display environment and then
        // re-execute ourselves via pkexec so we can open /dev/uinput.
        std::string me = get_exe_path(argc > 0 ? argv[0] : "moused");

        // Choose a private temp path
        std::string envfile = "/tmp/moused_env_" + std::to_string(getuid());

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
        chmod(envfile.c_str(), 0600);

        execlp("pkexec", "pkexec", me.c_str(), "--restore-env", (char *)NULL);

        // pkexec failed (not installed / user cancelled / no desktop session)
        // fallback: suggest manual sudo/doas
        unlink(envfile.c_str());
        throw std::runtime_error(
            "root required (need write access to /dev/uinput). "
            "polkit elevation failed — run with: sudo moused");
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

        std::string envfile;
        if (orig_uid != (uid_t)-1)
        {
            envfile = "/tmp/moused_env_" + std::to_string(orig_uid);
        }
        else
        {
            // Fallback: scan /tmp for moused_env_* files
            // For simplicity, try the most likely uid (1000 is common first desktop user)
            envfile = "/tmp/moused_env_1000";
        }

        std::ifstream f(envfile);
        if (f)
        {
            std::string line;
            while (std::getline(f, line))
            {
                // getline() strips the trailing newline
                auto eq = line.find('=');
                if (eq != std::string::npos)
                {
                    std::string key = line.substr(0, eq);
                    std::string val = line.substr(eq + 1);

                    if (key == "MU_ORIG_UID")
                    {
                        orig_uid = (uid_t)std::atoi(val.c_str());
                    }
                    else
                    {
                        // Restore environment variable (overwrite pkexec's root env
                        // with the original user's values)
                        setenv(key.c_str(), val.c_str(), 1);
                    }
                }
            }
            unlink(envfile.c_str());

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
        throw std::system_error(errno, std::generic_category(), "seteuid failed");
    }
}
#endif
# PlatformIO pre-build script (platformio.ini: extra_scripts = pre:...).
# Runs before the platform's own build script, on every build - including
# VS Code's Build button - and does two things:
#
# 1. Regenerates WebUI_gz.h from WebUI.h (tools/build_webui_gz.js), so the
#    gzipped page can't go stale. Needs Node.js; without it the compile's
#    static_assert still catches a stale page.
#
# 2. Keeps the build working when Windows blocks littlefs-python's native
#    module. The pioarduino platform imports it unconditionally at startup,
#    but only uses it to build LittleFS filesystem images, which this
#    project doesn't have. With Windows 11 Smart App Control on, loading
#    that unsigned DLL fails ("Un criterio di controllo dell'applicazione ha
#    bloccato il file" / "An Application Control policy has blocked this
#    file") and the whole build stops. If that happens, a placeholder module
#    takes its place; nothing is weakened - the blocked file stays blocked,
#    it simply isn't needed. Building a LittleFS image would then fail with
#    a clear message.

Import("env")  # noqa: F821 - provided by PlatformIO/SCons

import os
import shutil
import subprocess
import sys
import types

project_dir = env.subst("$PROJECT_DIR")  # noqa: F821

# Shows that PLATFORMIO_RUN_JOBS (.vscode/settings.json) / --jobs took effect.
print("Parallel compile jobs: %s" % env.GetOption("num_jobs"))  # noqa: F821

# --- 1. gzipped web page ------------------------------------------------------
node = shutil.which("node")
if node:
    rc = subprocess.call([node, os.path.join(project_dir, "tools", "build_webui_gz.js")])
    if rc != 0:
        sys.stderr.write("tools/build_webui_gz.js failed (exit %d)\n" % rc)
        env.Exit(1)  # noqa: F821
else:
    print("Note: node not found - WebUI_gz.h not regenerated (the build stops if it's stale)")

# --- 2. littlefs-python blocked by Windows --------------------------------------
try:
    from littlefs import lfs  # noqa: F401 - the native part, the one that gets blocked
except ImportError as err:
    reason = str(err)

    class LittleFS(object):
        def __init__(self, *args, **kwargs):
            raise RuntimeError(
                "littlefs-python can't be loaded on this PC (%s), so LittleFS images "
                "can't be built here. Firmware builds are unaffected." % reason)

    stub = types.ModuleType("littlefs")
    stub.LittleFS = LittleFS
    stub.lfs = types.ModuleType("littlefs.lfs")
    sys.modules["littlefs"] = stub
    sys.modules["littlefs.lfs"] = stub.lfs
    print("Note: littlefs-python unavailable (%s) - replaced by a placeholder; "
          "only filesystem-image builds need it." % reason)

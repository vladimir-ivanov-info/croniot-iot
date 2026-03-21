import os
Import("env")

ccache_dir = os.path.expanduser("~/.local/lib/ccache")
current_path = env["ENV"].get("PATH", "")
env["ENV"]["PATH"] = ccache_dir + ":" + current_path

#!/bin/sh
# Copies browser Doom into the router's RAM (/tmp/doom) and runs it there.
# While it runs, open http://<router-ip>:8666 in a browser. Ctrl+C stops it.
# Nothing is written to flash; a reboot wipes /tmp/doom. Run build.sh first.
# Usage: ./play.sh [root@192.168.1.1]    extra ssh options go in SSH_OPTS
set -e
B="$(cd "$(dirname "$0")" && pwd)/build"
R=${1:-root@192.168.1.1}
r() { ssh $SSH_OPTS "$@"; }

r "$R" 'mkdir -p /tmp/doom; killall doom-web 2>/dev/null; true'
r "$R" 'cat > /tmp/doom/doom-web && chmod +x /tmp/doom/doom-web' < "$B/doom-web"
r "$R" '[ -s /tmp/doom/freedoom1.wad ]' || r "$R" 'cat > /tmp/doom/freedoom1.wad' < "$B/freedoom1.wad"
r "$R" 'cat > /tmp/doom/.default.cfg' < "$B/.default.cfg"
echo "Open http://${R#*@}:8666 in your browser. Ctrl+C here stops the game."
r -t "$R" 'cd /tmp/doom && ./doom-web -iwad freedoom1.wad -scaling 1'

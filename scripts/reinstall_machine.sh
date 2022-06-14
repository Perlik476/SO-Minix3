#! /bin/bash

# Source: https://stackoverflow.com/a/24597941
function fail {
    printf '%s\n' "$1" >&2  # Send message to stderr.
    exit "${2-1}"  # Return a code specified by $2 or 1 by default.
}


# This script's directory.
dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
# Read config file.
. "${dir}/.config"

# Required commands.
commands=("ssh")
for c in "${commands[@]}"
do
if ! command -v "${c}" &> /dev/null
then
    fail "${c} could not be found"
fi
done

current_time=$(date "+%C%y%m%d%H%M.%S")
ssh root@localhost -p "${ssh_port}" "date ${current_time}"

ssh -p "${ssh_port}" root@localhost << EOF
cd /usr/src
make includes || exit 1
cd /usr/src/minix/kernel
make && make install || exit 1
cd /usr/src/minix/lib/libsys
make && make install || exit 1
cd /usr/src/minix/servers/sched
make && make install || exit 1
cd /usr/src/minix/servers/pm
make && make install || exit 1
cd /usr/src/lib/libc
make && make install || exit 1
cd /usr/src/releasetools
make do-hdboot

echo "Rebooting. You can exit with Ctrl+C"
reboot
EOF

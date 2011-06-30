#!/bin/sh -e
# (C) 2008 Canonical Ltd.
# Author: Martin Pitt <martin.pitt@ubuntu.com>
# License: GPL v2 or later
# modified by David D Lowe and Thomas Detoux
#
# Setup user and temporary home directory for guest session.
# If this succeeds, this script needs to print the username as last line to
# stdout.

USER="guest"

# if $USER already exists, it must be a locked system account with no existing
# home directory
if PWSTAT=`passwd -S "$USER"` 2>/dev/null; then
    if [ "`echo \"$PWSTAT\" | cut -f2 -d\ `" != "L" ]; then
        echo "User account $USER already exists and is not locked"
        exit 1
    fi
    PWENT=`getent passwd "$USER"` || {
        echo "getent passwd $USER failed"
        exit 1
    }
    GUEST_UID=`echo "$PWENT" | cut -f3 -d:`
    if [ "$GUEST_UID" -ge 500 ]; then
        echo "Account $USER is not a system user"
        exit 1
    fi
    HOME=`echo "$PWENT" | cut -f6 -d:`
    if [ "$HOME" != / ] && [ "${HOME#/tmp}" = "$HOME" ] && [ -d "$HOME" ]; then
        echo "Home directory of $USER already exists"
        exit 1
    fi
else
    # does not exist, so create it
    adduser --system --no-create-home --home / --gecos "Guest" --group --shell /bin/bash $USER || {
        umount "$HOME"
        rm -rf "$HOME"
        exit 1
    }
fi

# create temporary home directory
HOME=`mktemp -td guest-home.XXXXXX`
mount -t tmpfs -o mode=700 none "$HOME" || { rm -rf "$HOME"; exit 1; }
chown $USER:$USER "$HOME"
gs_skel=/etc/guest-session/skel
if [ -d "$gs_skel" ] && [ -n "`find $gs_skel -type f`" ]; then
    cp -rT $gs_skel "$HOME"
else
    cp -rT /etc/skel/ "$HOME"
fi
chown -R $USER:$USER "$HOME"
usermod -d "$HOME" "$USER"

#
# setup session
#

# disable screensaver, to avoid locking guest out of itself (no password)
su $USER <<EOF
gconftool-2 --set --type bool /desktop/gnome/lockdown/disable_lock_screen True
EOF

# disable some services that are unnecessary for the guest session
mkdir --parents "$HOME"/.config/autostart
cd /etc/xdg/autostart/
services="jockey-gtk.desktop update-notifier.desktop user-dirs-update-gtk.desktop"
for service in $services
do
    if [ -e /etc/xdg/autostart/"$service" ] ; then
        cp "$service" "$HOME"/.config/autostart
        echo "X-GNOME-Autostart-enabled=false" >> "$HOME"/.config/autostart/"$service"
    fi
done

# Load restricted session
#dmrc='[Desktop]\nSession=guest-restricted'
#/bin/echo -e "$dmrc" > "$HOME"/.dmrc

chown -R $USER:$USER "$HOME"

# set possible local guest session preferences
if [ -f /etc/guest-session/prefs.sh ]; then
    . /etc/guest-session/prefs.sh
fi

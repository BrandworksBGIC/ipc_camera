#!/bin/sh

CONF_DIR_JFFS2=/usr/conf
CONF_DIR_RAMFS=/var/conf

# Initialize $CONF_DIR_JFFS2
if [ -d /etc/conf ] && [ -d $CONF_DIR_JFFS2 ]; then
    cf=`ls /etc/conf`
    if [ -n "$cf" ]; then
        cd /etc/conf
        for i in *; do
            if [ ! -f $CONF_DIR_JFFS2/$i ]; then
                cp /etc/conf/$i $CONF_DIR_JFFS2/
            fi
        done
        cd -
    fi
    unset cf
fi

# Initialize $CONF_DIR_RAMFS
mkdir -p $CONF_DIR_RAMFS
if [ -d $CONF_DIR_JFFS2 ]; then
    cf=`ls $CONF_DIR_JFFS2`
    if [ -n "$cf" ]; then
        cp -rf $CONF_DIR_JFFS2/* $CONF_DIR_RAMFS/
    fi
fi


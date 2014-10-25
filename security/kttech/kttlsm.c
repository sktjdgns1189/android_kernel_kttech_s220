/*
 *  KTTech security module
 *
 *  This file contains the kttech hook function implementations.
 *
 *  Authors:
 *	namjja <namjja@kttech.co.kr>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License version 2,
 *      as published by the Free Software Foundation.
 */

#include <linux/xattr.h>
#include <linux/pagemap.h>
#include <linux/mount.h>
#include <linux/stat.h>
#include <linux/kd.h>
#include <asm/ioctls.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/pipe_fs_i.h>
#include <net/netlabel.h>
#include <net/cipso_ipv4.h>
#include <linux/audit.h>
#include <linux/magic.h>
#include <linux/dcache.h>

static char *check_rooting_cmd[] = {
	"init",
	"vold",
#ifndef KTTECH_FINAL_BUILD
	"adbd",
#endif
};

char *kttech_encode(const char *str)
{
    int len = 0;
    const char *p = str;
    char *cp;
    char *cp0;

    if (!p)
        return NULL;
    while (*p) {
        const unsigned char c = *p++;
        if (c == '\\')
            len += 2;
        else if (c > ' ' && c < 127)
            len++;
        else
            len += 4;
    }
    len++;
    /* Reserve space for appending "/". */
    cp = kzalloc(len + 10, GFP_NOFS);
    if (!cp)
        return NULL;
    cp0 = cp;
    p = str;
    while (*p) {
        const unsigned char c = *p++;

        if (c == '\\') {
            *cp++ = '\\';
            *cp++ = '\\';
        } else if (c > ' ' && c < 127) {
            *cp++ = c;
        } else {
            *cp++ = '\\';
            *cp++ = (c >> 6) + '0';
            *cp++ = ((c >> 3) & 7) + '0';
            *cp++ = (c & 7) + '0';
        }
    }
    return cp0;
}

char *realpath_from_path(struct path *path)
{
    char *buf = NULL;
    char *name = NULL;
    unsigned int buf_len = PAGE_SIZE / 2;
    struct dentry *dentry = path->dentry;
    bool is_dir;
    if (!dentry)
        return NULL;
    is_dir = dentry->d_inode && S_ISDIR(dentry->d_inode->i_mode);

	    while (1) {
        struct path ns_root = { .mnt = NULL, .dentry = NULL };
        char *pos;
        buf_len <<= 1;
        kfree(buf);
        buf = kmalloc(buf_len, GFP_NOFS);
        if (!buf)
            break;
        /* Get better name for socket. */
        if (dentry->d_sb && dentry->d_sb->s_magic == SOCKFS_MAGIC) {
            struct inode *inode = dentry->d_inode;
            struct socket *sock = inode ? SOCKET_I(inode) : NULL;
            struct sock *sk = sock ? sock->sk : NULL;
            if (sk) {
                snprintf(buf, buf_len - 1, "socket:[family=%u:"
                     "type=%u:protocol=%u]", sk->sk_family,
                     sk->sk_type, sk->sk_protocol);
            } else {
                snprintf(buf, buf_len - 1, "socket:[unknown]");
            }
            name = kttech_encode(buf);
            break;
        }
        /* For "socket:[\$]" and "pipe:[\$]". */
        if (dentry->d_op && dentry->d_op->d_dname) {
            pos = dentry->d_op->d_dname(dentry, buf, buf_len - 1);
            if (IS_ERR(pos))
                continue;
            name = kttech_encode(pos);
            break;
        }
        /* If we don't have a vfsmount, we can't calculate. */
        if (!path->mnt)
            break;
        /* go to whatever namespace root we are under */
        pos = __d_path(path, &ns_root, buf, buf_len);
        /* Prepend "/proc" prefix if using internal proc vfs mount. */
        if (!IS_ERR(pos) && (path->mnt->mnt_flags & MNT_INTERNAL) &&
            (path->mnt->mnt_sb->s_magic == PROC_SUPER_MAGIC)) {
            pos -= 5;
            if (pos >= buf)
                memcpy(pos, "/proc", 5);
            else
                pos = ERR_PTR(-ENOMEM);
        }
        if (IS_ERR(pos))
            continue;
        name = kttech_encode(pos);
        break;
    }
    kfree(buf);
    if (!name)
        printk("kttech : error\n");
    else if (is_dir && *name) {
        /* Append trailing '/' if dentry is a directory. */
        char *pos = name + strlen(name) - 1;
        if (*pos != '/')
            /*
             * This is OK because kttech_encode() reserves space
             * for appending "/".
             */
            *++pos = '/';
    }
    return name;
}

static int kttech_sb_mount(char *dev_name, struct path *path,
			  char *type, unsigned long flags, void *data)
{
	int i;
	char *argv[3];
	char *envp[3];

	if(!strcmp("/system/", realpath_from_path(path)) && current_uid() == 0){

		for(i=0; i < ARRAY_SIZE(check_rooting_cmd); i++){
			if(!strcmp(check_rooting_cmd[i], current->comm)){
//				printk("kttech : it is not rooting %s\n", check_rooting_cmd[i]);
				return 0;
			}
		}
	
//		printk("kttech : rooting\n");
		printk("kttech : [%s][%d] : %s[%s] %s[%s]\n", current->comm, 
					current_uid(), __func__, dev_name, 
					realpath_from_path(path), type);

		argv[0] = "/sbin/rd_aboot";
		argv[1] = "rooting";
		argv[2] = NULL;
		envp[0] = "HOME=/";
		envp[1] = "PATH=/sbin:/system/bin";
		envp[2] = NULL;
		call_usermodehelper(argv[0], argv, envp, 1);
#if 0
		if(flags & MS_REMOUNT)
			printk("kttech : remount\n");
#endif
	}
	return 0;
}

static int kttech_sb_umount(struct vfsmount *mnt, int flags)
{
//	printk("kttech : %s\n", __func__);
	return 0;
}

struct security_operations kttech_ops = {
	.name =				"kttech_lsm",

	.sb_mount = 			kttech_sb_mount,
	.sb_umount = 			kttech_sb_umount,

};


/**
 * kttech_init - initialize the KTTech lsm
 *
 * Returns 0
 */
static __init int kttech_init(void)
{

	printk(KERN_INFO "KTTech :  Initializing.\n");

	if (!security_module_enable(&kttech_ops))
		return 0;

	printk(KERN_INFO "KTTech :  enabled.\n");

	/*
	 * Register with LSM
	 */
	if (register_security(&kttech_ops))
		panic("kttech: Unable to register with kernel.\n");

	return 0;
}

security_initcall(kttech_init);

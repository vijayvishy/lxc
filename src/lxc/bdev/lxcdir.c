/*
 * lxc: linux Container library
 *
 * (C) Copyright IBM Corp. 2007, 2008
 *
 * Authors:
 * Daniel Lezcano <daniel.lezcano at free.fr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <string.h>

#include "bdev.h"
#include "log.h"
#include "utils.h"

lxc_log_define(lxcdir, lxc);

/*
 * for a simple directory bind mount, we substitute the old container
 * name and paths for the new
 */
int dir_clonepaths(struct bdev *orig, struct bdev *new, const char *oldname,
		const char *cname, const char *oldpath, const char *lxcpath,
		int snap, uint64_t newsize, struct lxc_conf *conf)
{
	int ret;
	size_t len;

	if (snap) {
		ERROR("directories cannot be snapshotted.  Try aufs or overlayfs.");
		return -1;
	}

	if (!orig->dest || !orig->src)
		return -1;

	len = strlen(lxcpath) + strlen(cname) + strlen("rootfs") + 4 + 3;
	new->src = malloc(len);
	if (!new->src)
		return -1;

	ret = snprintf(new->src, len, "dir:%s/%s/rootfs", lxcpath, cname);
	if (ret < 0 || (size_t)ret >= len)
		return -1;

	new->dest = strdup(new->src + 4);
	if (!new->dest)
		return -1;

	return 0;
}

int dir_create(struct bdev *bdev, const char *dest, const char *n,
	       struct bdev_specs *specs)
{
	int ret;
	const char *src;
	size_t len;

	/* strlen("dir:") */
	len = 4;
	if (specs && specs->dir)
		src = specs->dir;
	else
		src = dest;

	len += strlen(src) + 1;
	bdev->src = malloc(len);
	if (!bdev->src)
		return -1;

	ret = snprintf(bdev->src, len, "dir:%s", src);
	if (ret < 0 || (size_t)ret >= len)
		return -1;

	bdev->dest = strdup(dest);
	if (!bdev->dest)
		return -1;

	ret = mkdir_p(src, 0755);
	if (ret < 0) {
		ERROR("Failed to create %s", src);
		return -1;
	}

	ret = mkdir_p(bdev->dest, 0755);
	if (ret < 0) {
		ERROR("Failed to create %s", bdev->dest);
		return -1;
	}

	return 0;
}

int dir_destroy(struct bdev *orig)
{
	char *src;

	src = lxc_storage_get_path(orig->src, orig->src);

	if (lxc_rmdir_onedev(src, NULL) < 0)
		return -1;

	return 0;
}

int dir_detect(const char *path)
{
	if (!strncmp(path, "dir:", 4))
		return 1;

	if (is_dir(path))
		return 1;

	return 0;
}

int dir_mount(struct bdev *bdev)
{
	unsigned long mntflags;
	char *src, *mntdata;
	int ret;
	unsigned long mflags;

	if (strcmp(bdev->type, "dir"))
		return -22;

	if (!bdev->src || !bdev->dest)
		return -22;

	if (parse_mntopts(bdev->mntopts, &mntflags, &mntdata) < 0) {
		free(mntdata);
		return -22;
	}

	src = lxc_storage_get_path(bdev->src, bdev->type);

	ret = mount(src, bdev->dest, "bind", MS_BIND | MS_REC | mntflags, mntdata);
	if ((0 == ret) && (mntflags & MS_RDONLY)) {
		DEBUG("remounting %s on %s with readonly options",
			src ? src : "(none)", bdev->dest ? bdev->dest : "(none)");
		mflags = add_required_remount_flags(src, bdev->dest, MS_BIND | MS_REC | mntflags | MS_REMOUNT);
		ret = mount(src, bdev->dest, "bind", mflags, mntdata);
	}

	free(mntdata);
	return ret;
}

int dir_umount(struct bdev *bdev)
{
	if (strcmp(bdev->type, "dir"))
		return -22;

	if (!bdev->src || !bdev->dest)
		return -22;

	return umount(bdev->dest);
}

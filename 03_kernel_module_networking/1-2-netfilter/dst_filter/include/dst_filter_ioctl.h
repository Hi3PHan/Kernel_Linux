/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DST_FILTER_IOCTL_H
#define DST_FILTER_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define DST_FILTER_DEVICE_NAME "dst_filter"
#define DST_FILTER_DEVICE_PATH "/dev/dst_filter"

struct dst_filter_rule {
	__u8 enabled;
	__be32 dst_addr;
};

#define DST_FILTER_IOCTL_MAGIC 'D'
#define DST_FILTER_IOCTL_SET _IOW(DST_FILTER_IOCTL_MAGIC, 1, struct dst_filter_rule)
#define DST_FILTER_IOCTL_CLEAR _IO(DST_FILTER_IOCTL_MAGIC, 2)
#define DST_FILTER_IOCTL_GET _IOR(DST_FILTER_IOCTL_MAGIC, 3, struct dst_filter_rule)

#endif

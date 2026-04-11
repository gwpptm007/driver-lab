/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _DEMO_IOCTL_H
#define _DEMO_IOCTL_H
#include <linux/ioctl.h>

#define DEMO_MAGIC 'k'
#define DEMO_IOCTL_GET _IOR(DEMO_MAGIC, 1, int)
#define DEMO_IOCTL_SET _IOW(DEMO_MAGIC, 2, int)

#endif

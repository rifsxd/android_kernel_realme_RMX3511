/* include/linux/wakelock.h
 *
 * Copyright (C) 2007-2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _BF_FP_WAKELOCK_H
#define _BF_FP_WAKELOCK_H

#include <linux/ktime.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>

/* A wake_lock prevents the system from entering suspend or other low power
 * states when active. If the type is set to WAKE_LOCK_SUSPEND, the wake_lock
 * prevents a full system suspend.
 */

enum {
    WAKE_LOCK_SUSPEND, /* Prevent suspend */
    WAKE_LOCK_TYPE_COUNT
};

struct wake_lock {
    struct wakeup_source ws;
};

static inline void wake_lock_destroy(struct wakeup_source *ws)
{
    wakeup_source_unregister(ws);
}

static inline void wake_lock(struct wakeup_source *ws)
{
    __pm_stay_awake(ws);
}

static inline void wake_lock_timeout(struct wakeup_source *ws, long timeout)
{
    __pm_wakeup_event(ws, jiffies_to_msecs(timeout));
}

static inline void wake_unlock(struct wakeup_source *ws)
{
    __pm_relax(ws);
}

#endif


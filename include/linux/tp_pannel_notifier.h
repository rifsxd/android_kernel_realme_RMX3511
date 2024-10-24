/*******************************************************************************
** Copyright (C), 2008-2016, OPPO Mobile Comm Corp., Ltd.
** OPLUS_EDIT
** FILE: - Tp_suspend_notifier.h
** Description : This program is for Tp_suspend_notifier
** Version: 1.0
** Date : 2018/11/27
**
** -------------------------Revision History:----------------------------------
**  <author>	 <data> 	<version >			<desc>
**
**
*******************************************************************************/

#include <linux/notifier.h>
#include <linux/export.h>

enum TOUCHSCREEN_SLEEP_STATUS{
    TOUCHSCREEN_SUSPEND = 0,
    TOUCHSCREEN_RESUME = 1
};

//extern struct blocking_notifier_head tp_pannel_notifier_list;

int tp_pannel_register_client(struct notifier_block *nb);
int tp_pannel_unregister_client(struct notifier_block *nb);
int tp_pannel_notifier_call_chain(unsigned long val, void *v);
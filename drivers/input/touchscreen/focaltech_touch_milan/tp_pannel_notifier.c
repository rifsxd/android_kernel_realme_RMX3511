/*******************************************************************************
** Copyright (C), 2008-2016, OPPO Mobile Comm Corp., Ltd.
** OPLUS_EDIT
** FILE: - ilitek_v3.c
** Description : This program is for iliteck tp ic suspend & resume by lcd notifier
** Version: 1.0
** Date : 2018/11/27
**
** -------------------------Revision History:----------------------------------
**  <author>	 <data> 	<version >			<desc>
**
**
*******************************************************************************/

#include <linux/notifier.h>

BLOCKING_NOTIFIER_HEAD(tp_pannel_notifier_list);
EXPORT_SYMBOL_GPL(tp_pannel_notifier_list);

int tp_pannel_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&tp_pannel_notifier_list, nb);

}
EXPORT_SYMBOL(tp_pannel_register_client);

int tp_pannel_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&tp_pannel_notifier_list, nb);
}
EXPORT_SYMBOL(tp_pannel_unregister_client);

int tp_pannel_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&tp_pannel_notifier_list, val, v);

}
EXPORT_SYMBOL_GPL(tp_pannel_notifier_call_chain);
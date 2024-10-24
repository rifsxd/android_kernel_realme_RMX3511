#ifndef CTS_PROC_H
#define CTS_PROC_H

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

struct chipone_ts_data;

extern int cts_proc_init(struct chipone_ts_data *cts_data);
extern int cts_proc_deinit(struct chipone_ts_data *cts_data);

#endif /* CTS_PROC_H */


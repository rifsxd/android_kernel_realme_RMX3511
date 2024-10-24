#ifndef __QOS_STRUCT_DEF_H__
#define __QOS_STRUCT_DEF_H__

struct qos_reg {
	const char	*reg_name;
	u32 	 base_addr;
	u32 	 mask_value;
	u32 	 set_value;
	};

#endif
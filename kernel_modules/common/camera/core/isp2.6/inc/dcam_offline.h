
#ifndef _DCAM_OFFLINE_H_
#define _DCAM_OFFLINE_H_

int dcam_offline_slice_fmcu_cmds_set(struct dcam_fmcu_ctx_desc *fmcu, struct dcam_sw_context *pctx);
int dcam_offline_slice_info_cal(struct dcam_sw_context *pctx, struct camera_frame *pframe, uint32_t lbuf_width);
int dcam_offline_slices_proc(struct cam_hw_info *hw, struct dcam_sw_context *pctx, struct camera_frame *pframe);
int dcam_offline_param_set(struct cam_hw_info *hw, struct dcam_sw_context *pctx);
int dcam_offline_param_get(struct cam_hw_info *hw, struct dcam_sw_context *pctx,
			struct camera_frame *pframe);
uint32_t dcam_offline_slice_needed(struct cam_hw_info *hw, struct dcam_sw_context *pctx, uint32_t *dev_lbuf, uint32_t in_width);
struct camera_frame *dcam_offline_cycle_frame(struct dcam_sw_context *pctx);

#endif

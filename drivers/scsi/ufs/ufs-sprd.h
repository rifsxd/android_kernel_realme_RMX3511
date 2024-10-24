#ifndef _UFS_SPRD_H_
#define _UFS_SPRD_H_
#include "ufs-sprd.h"


static struct ufs_pa_layer_attr dts_pwr_info;
static struct ufs_pa_layer_attr compare_pwr_info;
static u32 times_pre_pwr;
static u32 times_pre_compare_fail;
static u32 times_post_pwr;
static u32 times_post_compare_fail;


int ufs_compare_dev_req_pwr_mode(struct ufs_hba *hba, struct ufs_pa_layer_attr *dev_req_params)
{
	struct ufs_pa_layer_attr  max_pwr_info_raw = {0};
	struct ufs_pa_layer_attr *max_pwr_info = &max_pwr_info_raw;
	struct ufs_pa_layer_attr *pwr_info = dev_req_params;


	/* Get the connected lane count */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&max_pwr_info->lane_rx);
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&max_pwr_info->lane_tx);

	if (pwr_info->pwr_tx == FAST_MODE)
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
				&max_pwr_info->gear_rx);
	else if (pwr_info->pwr_tx == SLOW_MODE)
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&max_pwr_info->gear_rx);

	if (pwr_info->pwr_rx == FAST_MODE)
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
				&max_pwr_info->gear_tx);
	else if (pwr_info->pwr_rx == SLOW_MODE)
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&max_pwr_info->gear_tx);

	memcpy(&(dts_pwr_info), pwr_info, sizeof(struct ufs_pa_layer_attr));
	memcpy(&(compare_pwr_info), max_pwr_info, sizeof(struct ufs_pa_layer_attr));

	/* if already configured to the requested pwr_mode */
	if (max_pwr_info->gear_rx < pwr_info->gear_rx  ||
			max_pwr_info->gear_tx < pwr_info->gear_tx  ||
			max_pwr_info->lane_rx < pwr_info->lane_rx  ||
			max_pwr_info->lane_tx < pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: the dev_req_pwr can not compare\n", __func__);
		dev_err(hba->dev, "dts_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
				pwr_info->gear_rx, pwr_info->gear_tx,
				pwr_info->lane_rx, pwr_info->lane_tx, pwr_info->pwr_rx,
				pwr_info->pwr_tx, pwr_info->hs_rate);
		dev_err(hba->dev, "compare_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
				max_pwr_info->gear_rx, max_pwr_info->gear_tx,
				max_pwr_info->lane_rx, max_pwr_info->lane_tx,
				max_pwr_info->pwr_rx, max_pwr_info->pwr_tx,
				max_pwr_info->hs_rate);
		return -EINVAL;
	}

	return 0;
}

int ufs_compare_max_pwr_mode(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr  pwr_info_raw;
	struct ufs_pa_layer_attr *pwr_info = &pwr_info_raw;
	struct ufs_pa_layer_attr *max_pwr_info = &hba->max_pwr_info.info;

	if (!hba->max_pwr_info.is_valid && (max_pwr_info->pwr_tx != 1))
		return -EINVAL;

	pwr_info->pwr_tx = FAST_MODE;
	pwr_info->pwr_rx = FAST_MODE;
	pwr_info->hs_rate = PA_HS_MODE_B;

	/* Get the connected lane count */
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDRXDATALANES),
			&pwr_info->lane_rx);
	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_CONNECTEDTXDATALANES),
			&pwr_info->lane_tx);

	if (!pwr_info->lane_rx || !pwr_info->lane_tx) {
		dev_err(hba->dev, "%s: invalid connected lanes value. rx=%d, tx=%d\n",
				__func__,
				pwr_info->lane_rx,
				pwr_info->lane_tx);
		return -EINVAL;
	}

	ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR), &pwr_info->gear_rx);
	if (!pwr_info->gear_rx) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_rx);
		if (!pwr_info->gear_rx) {
			dev_err(hba->dev, "%s: invalid max pwm rx gear read = %d\n",
					__func__, pwr_info->gear_rx);
			return -EINVAL;
		}
		pwr_info->pwr_rx = SLOW_MODE;
	}

	ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXHSGEAR),
			&pwr_info->gear_tx);
	if (!pwr_info->gear_tx) {
		ufshcd_dme_peer_get(hba, UIC_ARG_MIB(PA_MAXRXPWMGEAR),
				&pwr_info->gear_tx);
		if (!pwr_info->gear_tx) {
			dev_err(hba->dev, "%s: invalid max pwm tx gear read = %d\n",
					__func__, pwr_info->gear_tx);
			return -EINVAL;
		}
		pwr_info->pwr_tx = SLOW_MODE;
	}
	memcpy(&(dts_pwr_info), max_pwr_info, sizeof(struct ufs_pa_layer_attr));
	memcpy(&(compare_pwr_info), pwr_info, sizeof(struct ufs_pa_layer_attr));

	/* if already configured to the requested pwr_mode */
	if (pwr_info->gear_rx != max_pwr_info->gear_rx  ||
			pwr_info->gear_tx != max_pwr_info->gear_tx  ||
			pwr_info->lane_rx != max_pwr_info->lane_rx   ||
			pwr_info->lane_tx != max_pwr_info->lane_tx   ||
			pwr_info->pwr_rx != max_pwr_info->pwr_rx     ||
			pwr_info->pwr_tx != max_pwr_info->pwr_tx     ||
			pwr_info->hs_rate != max_pwr_info->hs_rate) {
		dev_err(hba->dev, "%s: the max can not compare\n", __func__);
		dev_err(hba->dev, "compare_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
				pwr_info->gear_rx, pwr_info->gear_tx,
				pwr_info->lane_rx, pwr_info->lane_tx, pwr_info->pwr_rx,
				pwr_info->pwr_tx, pwr_info->hs_rate);
		dev_err(hba->dev, "dts_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
				max_pwr_info->gear_rx, max_pwr_info->gear_tx,
				max_pwr_info->lane_rx, max_pwr_info->lane_tx, max_pwr_info->pwr_rx,
				max_pwr_info->pwr_tx, max_pwr_info->hs_rate);
		return -EINVAL;
	}
	return 0;
}

int ufs_sprd_pwr_post_compare(struct ufs_hba *hba)
{
	struct ufs_pa_layer_attr  pwr_info_raw = {0};
	struct ufs_pa_layer_attr *pwr_mode = &pwr_info_raw;
	int ret = 0, pwr = 0;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_RXGEAR),
			&pwr_mode->gear_rx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_TXGEAR),
			&pwr_mode->gear_tx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_ACTIVERXDATALANES),
			&pwr_mode->lane_rx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_ACTIVETXDATALANES),
			&pwr_mode->lane_tx);
	if (ret)
		goto out;

	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_HSSERIES),
			&pwr_mode->hs_rate);
	if (ret)
		goto out;
	ret = ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE),
			&pwr);
	if (ret)
		goto out;
	pwr_mode->pwr_rx = (pwr >> 4) & 0xf;
	pwr_mode->pwr_tx = (pwr >> 0) & 0xf;
	if (pwr_mode->gear_rx == dts_pwr_info.gear_rx &&
			pwr_mode->gear_tx == dts_pwr_info.gear_tx &&
			pwr_mode->lane_rx == dts_pwr_info.lane_rx &&
			pwr_mode->lane_tx == dts_pwr_info.lane_tx &&
			pwr_mode->pwr_rx == dts_pwr_info.pwr_rx &&
			pwr_mode->pwr_tx == dts_pwr_info.pwr_tx &&
			pwr_mode->hs_rate == dts_pwr_info.hs_rate){
		return 0;
	}
out:
	dev_err(hba->dev, "sprd_pwr_post can not compare\n");
	dev_err(hba->dev, "compare %d,%d,%d,%d,%d,%d,%d\n",
			pwr_mode->gear_rx, pwr_mode->gear_tx,
			pwr_mode->lane_rx, pwr_mode->lane_tx, pwr_mode->pwr_rx,
			pwr_mode->pwr_tx, pwr_mode->hs_rate);
	dev_err(hba->dev, "dts_pwr_info %d,%d,%d,%d,%d,%d,%d\n",
			dts_pwr_info.gear_rx, dts_pwr_info.gear_tx,
			dts_pwr_info.lane_rx, dts_pwr_info.lane_tx,
			dts_pwr_info.pwr_rx,
			dts_pwr_info.pwr_tx, dts_pwr_info.hs_rate);
	return -1;
}

int  ufs_sprd_pwr_change_compare(struct ufs_hba *hba,
		enum ufs_notify_change_status status,
		struct ufs_pa_layer_attr *dev_max_params,
		struct ufs_pa_layer_attr *dev_req_params,
		int err)
{
	switch (status) {
	case PRE_CHANGE:
		times_pre_pwr++;
		if (err == 0) {
			if (ufs_compare_dev_req_pwr_mode(hba, dev_req_params)) {
				times_pre_compare_fail++;
				dev_err(hba->dev, "dev_req_pwr_mode err: compare_pwr\n");
#if defined(CONFIG_SPRD_DEBUG)
				panic("pre_compare_fail");
#endif
			}
		} else {
			if (ufs_compare_max_pwr_mode(hba)) {
				times_pre_compare_fail++;
				dev_err(hba->dev, "max_pwr_mode err:  compare_max_pwr\n");
#if defined(CONFIG_SPRD_DEBUG)
				panic("pre_compare_fail");
#endif
			}
		}
		break;
	case POST_CHANGE:
		times_post_pwr++;
		/* if already configured to the requested pwr_mode */
		if (ufs_sprd_pwr_post_compare(hba)) {
			times_post_compare_fail++;
			dev_err(hba->dev, "%s: power configured error\n", __func__);
#if defined(CONFIG_SPRD_DEBUG)
			panic("post_compare_fail");
#endif
		}
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}
#endif/* _UFS_SPRD_H_ */

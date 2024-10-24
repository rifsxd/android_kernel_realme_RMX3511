#ifndef CTS_OEM_H
#define CTS_OEM_H

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include "cts_core.h"

struct chipone_ts_data;

struct cts_oem_data {
    struct proc_dir_entry *selftest_proc_entry;
    struct proc_dir_entry *black_selftest_proc_entry;

    bool test_config_from_dt_has_parsed;
    bool black_test_config_from_dt_has_parsed;

    /* Test configuration from device tree */
    bool test_reset_pin;
    int  reset_pin_test_result;

    bool test_int_pin;
    int  int_pin_test_result;

    bool test_rawdata;
    u32  rawdata_test_frames;
    int  rawdata_test_result;
    u16 *rawdata_test_data;
    int  rawdata_test_data_buff_size;
    int  rawdata_test_data_wr_size;
    int  rawdata_min;
    int  rawdata_max;

    bool test_noise;
    u32  noise_test_frames;
    int  noise_test_result;
    u16 *noise_test_data;
    int  noise_test_data_buff_size;
    int  noise_test_data_wr_size;
    int  noise_max;

    bool test_open;
    int  open_test_result;
    u16 *open_test_data;
    int  open_test_data_buff_size;
    int  open_test_data_wr_size;
    int  open_min;

    bool test_short;
    int  short_test_result;
    u16 *short_test_data;
    int  short_test_data_buff_size;
    int  short_test_data_wr_size;
    int  short_min;

    bool test_comp_cap;
    int  comp_cap_test_result;
    u8  *comp_cap_test_data;
    int  comp_cap_test_data_buff_size;
    int  comp_cap_test_data_wr_size;
    int  comp_cap_min;
    int  comp_cap_max;
	
#ifdef CONFIG_CTS_GESTURE_TEST
    bool test_gesture_rawdata;
    u32  gesture_rawdata_test_frames;
    int  gesture_rawdata_test_result;
    int  gesture_rawdata_min;
    int  gesture_rawdata_max;
    u16 *gesture_rawdata_test_data;
    int  gesture_rawdata_test_data_buff_size;
    int  gesture_rawdata_test_data_wr_size;

    bool test_gesture_lp_rawdata;
    u32  gesture_lp_rawdata_test_frames;
	int  gesture_lp_rawdata_test_result;
	int  gesture_lp_rawdata_min;
	int  gesture_lp_rawdata_max;
    u16 *gesture_lp_rawdata_test_data;
    int  gesture_lp_rawdata_test_data_buff_size;
    int  gesture_lp_rawdata_test_data_wr_size;
	
    bool test_gesture_noise;
    u32  gesture_noise_test_frames;
    int  gesture_noise_test_result;
    int  gesture_noise_max;
    u16 *gesture_noise_test_data;
    int  gesture_noise_test_data_buff_size;
    int  gesture_noise_test_data_wr_size;

    bool test_gesture_lp_noise;
    u32  gesture_lp_noise_test_frames;
	int  gesture_lp_noise_test_result;	
    int  gesture_lp_noise_max;
    u16 *gesture_lp_noise_test_data;
    int  gesture_lp_noise_test_data_buff_size;
    int  gesture_lp_noise_test_data_wr_size;
#endif

    struct chipone_ts_data *cts_data;

};

/* Default settings if device tree NOT exist */
#define OEM_OF_DEF_PROPVAL_RAWDATA_FRAMES   1
#define OEM_OF_DEF_PROPVAL_RAWDATA_MIN      1400
#define OEM_OF_DEF_PROPVAL_RAWDATA_MAX      2600
#define OEM_OF_DEF_PROPVAL_NOISE_FRAMES     16
#define OEM_OF_DEF_PROPVAL_NOISE_MAX        50
#define OEM_OF_DEF_PROPVAL_OPEN_MIN         200
#define OEM_OF_DEF_PROPVAL_SHORT_MIN        200
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MIN     20
#define OEM_OF_DEF_PROPVAL_COMP_CAP_MAX     110
#ifdef CONFIG_CTS_GESTURE_TEST
#define OEM_OF_DEF_PROPVAL_GESTURE_RAWDATA_FRAMES     3
#define OEM_OF_DEF_PROPVAL_GESTURE_RAWDATA_MIN        1400
#define OEM_OF_DEF_PROPVAL_GESTURE_RAWDATA_MAX        2600
#define OEM_OF_DEF_PROPVAL_GESTURE_LP_RAWDATA_FRAMES  3
#define OEM_OF_DEF_PROPVAL_GESTURE_LP_RAWDATA_MIN     1400
#define OEM_OF_DEF_PROPVAL_GESTURE_LP_RAWDATA_MAX     2600
#define OEM_OF_DEF_PROPVAL_GESTURE_NOISE_FRAMES       3
#define OEM_OF_DEF_PROPVAL_GESTURE_NOISE_MAX          100
#define OEM_OF_DEF_PROPVAL_GESTURE_LP_NOISE_FRAMES    3
#define OEM_OF_DEF_PROPVAL_GESTURE_LP_NOISE_MAX       60
#endif


/* Following options override device tree settings */
#define OEM_OF_DEF_PROPVAL_TEST_RESET_PIN   true
#define OEM_OF_DEF_PROPVAL_TEST_INT_PIN     false
#define OEM_OF_DEF_PROPVAL_TEST_RAWDATA     true
#define OEM_OF_DEF_PROPVAL_TEST_NOISE       true
#define OEM_OF_DEF_PROPVAL_TEST_OPEN        true
#define OEM_OF_DEF_PROPVAL_TEST_SHORT       true
#define OEM_OF_DEF_PROPVAL_TEST_COMP_CAP    true
#ifdef  CONFIG_CTS_GESTURE_TEST
#define OEM_OF_DEF_PROPVAL_TEST_GESTURE_RAWDATA    true
#define OEM_OF_DEF_PROPVAL_TEST_GESTURE_LP_RAWDATA true
#define OEM_OF_DEF_PROPVAL_TEST_GESTURE_NOISE      true
#define OEM_OF_DEF_PROPVAL_TEST_GESTURE_LP_NOISE   true
#endif

#define OEM_OF_PROPNAME_PREFIX                 "chipone," //modify

#define OEM_OF_PROPNAME_TEST_RESET_PIN      OEM_OF_PROPNAME_PREFIX"test-reset-pin"
#define OEM_OF_PROPNAME_TEST_INT_PIN        OEM_OF_PROPNAME_PREFIX"test-int-pin"
#define OEM_OF_PROPNAME_TEST_RAWDATA        OEM_OF_PROPNAME_PREFIX"test-rawdata"
#define OEM_OF_PROPNAME_RAWDATA_FRAMES      OEM_OF_PROPNAME_PREFIX"test-rawdata-frames"
#define OEM_OF_PROPNAME_RAWDATA_MIN         OEM_OF_PROPNAME_PREFIX"rawdata-min"
#define OEM_OF_PROPNAME_RAWDATA_MAX         OEM_OF_PROPNAME_PREFIX"rawdata-max"
#define OEM_OF_PROPNAME_TEST_NOISE          OEM_OF_PROPNAME_PREFIX"test-noise"
#define OEM_OF_PROPNAME_NOISE_FRAMES        OEM_OF_PROPNAME_PREFIX"test-noise-frames"
#define OEM_OF_PROPNAME_NOISE_MAX           OEM_OF_PROPNAME_PREFIX"noise-max"
#define OEM_OF_PROPNAME_TEST_OPEN           OEM_OF_PROPNAME_PREFIX"test-open"
#define OEM_OF_PROPNAME_OPEN_MIN            OEM_OF_PROPNAME_PREFIX"open-min"
#define OEM_OF_PROPNAME_TEST_SHORT          OEM_OF_PROPNAME_PREFIX"test-short"
#define OEM_OF_PROPNAME_SHORT_MIN           OEM_OF_PROPNAME_PREFIX"short-min"
#define OEM_OF_PROPNAME_TEST_COMP_CAP       OEM_OF_PROPNAME_PREFIX"test-compensate-cap"
#define OEM_OF_PROPNAME_COMP_CAP_MIN        OEM_OF_PROPNAME_PREFIX"compensate-cap-min"
#define OEM_OF_PROPNAME_COMP_CAP_MAX        OEM_OF_PROPNAME_PREFIX"compensate-cap-max"
#ifdef  CONFIG_CTS_GESTURE_TEST
#define OEM_OF_PROPNAME_TEST_GESTURE_RAWDATA       OEM_OF_PROPNAME_PREFIX"test-gesture-rawdata"
#define OEM_OF_PROPNAME_GESTURE_RAWDATA_FRAMES     OEM_OF_PROPNAME_PREFIX"test-gesture-rawdata-frames"
#define OEM_OF_PROPNAME_GESTURE_RAWDATA_MIN        OEM_OF_PROPNAME_PREFIX"gesture-rawdata-min"
#define OEM_OF_PROPNAME_GESTURE_RAWDATA_MAX        OEM_OF_PROPNAME_PREFIX"gesture-rawdata-max"
#define OEM_OF_PROPNAME_TEST_GESTURE_LP_RAWDATA    OEM_OF_PROPNAME_PREFIX"test-gesture-lp-rawdata"
#define OEM_OF_PROPNAME_GESTURE_LP_RAWDATA_FRAMES  OEM_OF_PROPNAME_PREFIX"test-gesture-lp-rawdata-frames"
#define OEM_OF_PROPNAME_GESTURE_LP_RAWDATA_MIN     OEM_OF_PROPNAME_PREFIX"gesture-lp-rawdata-min"
#define OEM_OF_PROPNAME_GESTURE_LP_RAWDATA_MAX     OEM_OF_PROPNAME_PREFIX"gesture-lp-rawdata-max"
#define OEM_OF_PROPNAME_TEST_GESTURE_NOISE         OEM_OF_PROPNAME_PREFIX"test-gesture-noise"
#define OEM_OF_PROPNAME_GESTURE_NOISE_FRAMES       OEM_OF_PROPNAME_PREFIX"test-gesture-noise-frames"
#define OEM_OF_PROPNAME_GESTURE_NOISE_MAX          OEM_OF_PROPNAME_PREFIX"gesture-noise-max"
#define OEM_OF_PROPNAME_TEST_GESTURE_LP_NOISE      OEM_OF_PROPNAME_PREFIX"test-gesture-lp-noise"
#define OEM_OF_PROPNAME_GESTURE_LP_NOISE_FRAMES    OEM_OF_PROPNAME_PREFIX"test-gesture-lp-noise-frames"
#define OEM_OF_PROPNAME_GESTURE_LP_NOISE_MAX       OEM_OF_PROPNAME_PREFIX"gesture-lp-noise-max"
#endif

#define CTS_PROC_TOUCHPANEL_FOLDER "touchpanel"
extern struct proc_dir_entry *CTS_proc_touchpanel_dir;
extern int touch_black_test;

#ifdef CONFIG_CTS_GESTURE_TEST
int parse_black_selftest_dt(struct cts_oem_data *oem_data, struct device_node *np);
//static void do_black_selftest(struct cts_device *cts_dev, struct cts_oem_data *oem_data);
extern int prepare_black_test(struct cts_device *cts_dev);
extern int alloc_black_selftest_data_mem(struct cts_oem_data *oem_data, int nodes);
int dump_tsdata_to_csv_file(const char *filepath, int flags,
    const u16 *data, int frames, int rows, int cols);
#endif

extern int cts_oem_init(struct chipone_ts_data *cts_data);
extern int cts_oem_deinit(struct chipone_ts_data *cts_data);

#endif /* CTS_VENDOR_H */


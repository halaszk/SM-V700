/* linux/drivers/video/samsung/ssd_s6e63j0x03.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com

 * Samsung Smart Dimming for OCTA
 *
 * Minwoo Kim, <minwoo7945.kim@samsung.com>
 *
*/

#include <linux/module.h>

#include "ssd_s6e63j0x03_tbl.h"
#include "ssd.h"

//#define DEBUG 
#define CONFIG_REF_SHIFT
#define CONFIG_COLOR_SHIFT


int read_mtp(struct dim_data *data, const unsigned char *reg)
{
	int i, j;
	int index = 0;

	for (i = 0; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				data->mtp_gamma[i][j] = reg[index] << 8 | reg[index+1];
				index += 2;
			} else { 
				data->mtp_gamma[i][j] = reg[index];
				index++;
			}
		}
	}
#ifdef DEBUG
	printk("====== READ GAMMA ======\n");
	for (i = 0; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			printk("0x%03x  ", data->mtp_gamma[i][j]);	
		}
		printk("\n");
	}
#endif
	return 0;
}


static int calc_vt_volt(int gamma)
{
	int max;

	max = sizeof(vt_trans_volt) >> 2;
	if (gamma > max) {
		gamma = max;
	}

	return (int)vt_trans_volt[gamma];
}

static int calc_v0_volt(struct dim_data *data, int color)
{
	return MULTIPLE_VREGOUT;
}


static int calc_v5_volt(struct dim_data *data, int color)
{
	int vt, ret, v15, gamma;
	
	gamma = data->mtp_gamma[V5][color];
	
	if (gamma >= vreg_element_max[V5]) {
		gamma = vreg_element_max[V5];
	}

	vt = data->volt_vt[color];
	v15 = data->volt[TBL_INDEX_V15][color];
	ret = (vt << 10) - ((vt - v15) * (int)v5_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v15_volt(struct dim_data *data, int color)
{
	int vt, ret, v31, gamma;

	gamma = data->mtp_gamma[V15][color];

	if (gamma >= vreg_element_max[V15]) {
		gamma = vreg_element_max[V15];
	}

	vt = data->volt_vt[color];
	v31 = data->volt[TBL_INDEX_V31][color];
	ret = (vt << 10) - ((vt - v31) * (int)v15_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v31_volt(struct dim_data *data, int color)
{
	int vt, ret, v63, gamma;

	gamma = data->mtp_gamma[V31][color];
	
	if (gamma >= vreg_element_max[V31]) {
		gamma = vreg_element_max[V31];
	}
	
	vt = data->volt_vt[color];
	v63 = data->volt[TBL_INDEX_V63][color];
	ret = (vt << 10) - ((vt - v63) * (int)v31_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v63_volt(struct dim_data *data, int color)
{
	int vt, ret, v127, gamma;

	gamma = data->mtp_gamma[V63][color];

	if (gamma >= vreg_element_max[V63]) {
		gamma = vreg_element_max[V63];
	}
	
	vt = data->volt_vt[color];
	v127 = data->volt[TBL_INDEX_V127][color];
	ret = (vt << 10) - ((vt - v127) * (int)v63_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v127_volt(struct dim_data *data, int color)
{
	int vt, ret, v191, gamma;

	gamma = data->mtp_gamma[V127][color];
	
	if (gamma >= vreg_element_max[V127]) {
		gamma = vreg_element_max[V127];
	}

	vt = data->volt_vt[color];
	v191 = data->volt[TBL_INDEX_V191][color];
	ret = (vt << 10) - ((vt - v191) * (int)v127_trans_volt[gamma]);

	return ret >> 10;
}

static int calc_v191_volt(struct dim_data *data, int color)
{
	int vt, ret, v255, gamma;
	
	gamma = data->mtp_gamma[V191][color];

	if (gamma >= vreg_element_max[V191]) {
		gamma = vreg_element_max[V191];
	}	

	vt = data->volt_vt[color];

	v255 = data->volt[TBL_INDEX_V255][color];

	ret = (vt << 10) - ((vt - v255) * (int)v191_trans_volt[gamma]); 

	return ret >> 10;
}

static int calc_v255_volt(struct dim_data *data, int color)
{
	int ret, gamma;
	
	gamma = data->mtp_gamma[V255][color];

	if (gamma >= vreg_element_max[V255]) {
		gamma = vreg_element_max[V255];
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}


static int calc_inter_v0_v5(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v0, v5, ratio;

	ratio = (int)int_tbl_v0_v5[gray];

	v0 = data->volt[TBL_INDEX_V0][color];
	v5 = data->volt[TBL_INDEX_V5][color];
	
	ret = (v0 << 10) - ((v0 - v5) * ratio);

	return ret >> 10;
}

static int calc_inter_v5_v15(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v5, v15, ratio;

	ratio = (int)int_tbl_v5_v15[gray];
	v5 = data->volt[TBL_INDEX_V5][color];
	v15 = data->volt[TBL_INDEX_V15][color];

	ret = (v5 << 10) - ((v5 - v15) * ratio);

	return ret >> 10;
}

static int calc_inter_v15_v31(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v15, v31, ratio;

	ratio = (int)int_tbl_v15_v31[gray];
	v15 = data->volt[TBL_INDEX_V15][color];
	v31 = data->volt[TBL_INDEX_V31][color];

	ret = (v15 << 10) - ((v15 - v31) * ratio);

	return ret >> 10;
}

static int calc_inter_v31_v63(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v31, v63, ratio;

	ratio = (int)int_tbl_v31_v63[gray];
	v31 = data->volt[TBL_INDEX_V31][color];
	v63 = data->volt[TBL_INDEX_V63][color];

	ret = (v31 << 10) - ((v31 - v63) * ratio);

	return ret >> 10;
}

static int calc_inter_v63_v127(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v63, v127, ratio;
	
	ratio = (int)int_tbl_v63_v127[gray];
	v63 = data->volt[TBL_INDEX_V63][color];
	v127 = data->volt[TBL_INDEX_V127][color];

	ret = (v63 << 10) - ((v63 - v127) * ratio);

	return ret >> 10;
}

static int calc_inter_v127_v191(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v127, v191, ratio;
	
	ratio = (int)int_tbl_v127_v191[gray];
	v127 = data->volt[TBL_INDEX_V127][color];
	v191 = data->volt[TBL_INDEX_V191][color];

	ret = (v127 << 10) - ((v127 - v191) * ratio);

	return ret >> 10;
}

static int calc_inter_v191_v255(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v191, v255, ratio;

	ratio = (int)int_tbl_v191_v255[gray];
	v191 = data->volt[TBL_INDEX_V191][color];
	v255 = data->volt[TBL_INDEX_V255][color];
	
	ret = (v191 << 10) - ((v191 - v255) * ratio);

	return ret >> 10;
}

int generate_volt_table(struct dim_data *data)
{
	int i, j;
	int seq, index, gray;
	int ret = 0;
	int calc_seq[NUM_VREF] = {V0, V255, V191, V127, V63, V31, V15, V5};
	int (*calc_volt_point[NUM_VREF])(struct dim_data *, int) = {
		calc_v0_volt,
		calc_v5_volt,
		calc_v15_volt,
		calc_v31_volt,
		calc_v63_volt,
		calc_v127_volt,
		calc_v191_volt,
		calc_v255_volt,
	};
	int (*calc_inter_volt[NUM_VREF])(struct dim_data *, int, int)  = {
		NULL,
		calc_inter_v0_v5,
		calc_inter_v5_v15,
		calc_inter_v15_v31,
		calc_inter_v31_v63,
		calc_inter_v63_v127,
		calc_inter_v127_v191,
		calc_inter_v191_v255,
	};

	for (i = 0; i < CI_MAX; i++) {
		data->volt_vt[i] = calc_vt_volt(data->mtp_gamma[VT][i]);
	}
	/* calculate voltage for every vref point */
	for (j = 0; j < NUM_VREF; j++) {
		seq = calc_seq[j];
 		index = vref_index[seq];
		if (calc_volt_point[seq] != NULL) {
			for (i = 0; i < CI_MAX; i++)
					data->volt[index][i] = calc_volt_point[seq](data ,i);
		}
	}

	/* */
	index = 0;
	for (i = 0; i < MAX_GRADATION; i++) {
		if (i == vref_index[index]) {
			index++;
			continue;
		}
		gray = (i - vref_index[index - 1]) - 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_inter_volt[index] != NULL) {
				data->volt[i][j] = calc_inter_volt[index](data, gray, j);
			}
		}
	
	}

#ifdef DEBUG
	printk("=========================== VT Voltage ===========================\n"
);
	
	printk("R : %05d : G : %05d : B : %05d ",
		data->mtp_gamma[VT][0], 
		data->mtp_gamma[VT][1], 
		data->mtp_gamma[VT][2]);
	
	printk("\n=================================================================\n");

	for (i = 0; i < MAX_GRADATION; i++) {
		for (j = 0; j < CI_MAX; j++)
		printk("V%03d R : %05d G : %05d B : %05d \n", i,
								data->volt[i][0], 
								data->volt[i][1], 
								data->volt[i][2]);
	}
#endif
	return ret;
}


static int lookup_volt_index(struct dim_data *data, int gray)
{
	int ret, i;
	int temp;
	int index;
	int index_l, index_h, exit;
	int cnt_l, cnt_h;
	int p_delta, delta;

	temp = gray >> 20;
	index = (int)lookup_tbl[temp];
#ifdef DEBUG
	printf("==================== look up index ====================\n");
	printf("gray : %d : %d, index : %d\n", gray, temp, index);
#endif
	exit = 1;
	i = 0;
	while(exit) {
		index_l = temp - i;
		index_h = temp + i;
		if (index_l < 0)
			index_l = 0;
		if (index_h > MAX_BRIGHTNESS)
			index_h = MAX_BRIGHTNESS;
		cnt_l = (int)lookup_tbl[index] - (int)lookup_tbl[index_l];
		cnt_h = (int)lookup_tbl[index_h] - (int)lookup_tbl[index];
		
		if (cnt_l + cnt_h) {
			exit = 0;
		}
		i++;
	}
#ifdef DEBUG
	printf("base index : %d, cnt : %d\n", lookup_tbl[index_l], cnt_l + cnt_h);
#endif
	p_delta = 0;
	index = (int)lookup_tbl[index_l];
	ret = index;
	
	temp = gamma_multi_tbl[index] << 10;

	if (gray > temp)
		p_delta = gray - temp; 
	else 
		p_delta = temp - gray;
#ifdef DEBUG
	printf("temp : %d, gray : %d, p_delta : %d\n", temp, gray, p_delta);
#endif
	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		temp = gamma_multi_tbl[index + i] << 10;
		if (gray > temp)
			delta = gray - temp; 
		else 
			delta = temp - gray;
#ifdef DEBUG	
		printf("temp : %d, gray : %d, delta : %d\n", temp, gray, delta);
#endif
		if (delta < p_delta) {
			p_delta = delta;
			ret = index + i;
		} 
	}
#ifdef DEBUG
	printf("ret : %d\n", ret);
#endif
	return ret;
}


static int calc_reg_v5(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;

	t1 = data->look_volt[V5][color] - MULTIPLE_VREGOUT;
	t2 = data->look_volt[V15][color] - data->volt_vt[color];

	ret = (((t1) * (fix_const[V5].de)) / t2) - fix_const[V5].nu;
	
	return ret;
}



static int calc_reg_v15(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;

	t1 = data->look_volt[V15][color] - data->volt_vt[color];
	t2 = data->look_volt[V31][color] - data->volt_vt[color];

	ret = (((t1) * (fix_const[V15].de)) / t2) - fix_const[V15].nu;
	
	return ret;

}

static int calc_reg_v31(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;

	t1 = data->look_volt[V31][color] - data->volt_vt[color];
	t2 = data->look_volt[V63][color] - data->volt_vt[color];

	ret = (((t1) * (fix_const[V31].de)) / t2) - fix_const[V31].nu;
	
	return ret;

}

static int calc_reg_v63(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;

	t1 = data->look_volt[V63][color] - data->volt_vt[color];
	t2 = data->look_volt[V127][color] - data->volt_vt[color];

	ret = (((t1) * (fix_const[V63].de)) / t2) - fix_const[V63].nu;

	return ret;
}


static int calc_reg_v127(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;

	t1 = data->look_volt[V127][color] - data->volt_vt[color];
	t2 = data->look_volt[V191][color] - data->volt_vt[color];

	ret = (((t1) * (fix_const[V127].de)) / t2) - fix_const[V127].nu;

	return ret;
}


static int calc_reg_v191(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;

	t1 = data->look_volt[V191][color] - data->volt_vt[color];
	t2 = data->look_volt[V255][color] - data->volt_vt[color];

	ret = (((t1) * (fix_const[V191].de)) / t2) - fix_const[V191].nu;

	return ret;
}


static int calc_reg_v255(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;

	t1 = MULTIPLE_VREGOUT -  data->look_volt[V255][color];

	ret = ((t1 * fix_const[V255].de) / MULTIPLE_VREGOUT)  - fix_const[V255].nu;
	
	return ret;

}


int get_gamma(struct dim_data *data, int br, unsigned char *result)
{
	int i, j;
	int ret = 0;
	int gray, index, shift, c_shift;
	int gamma_int[NUM_VREF][CI_MAX];
	int (*calc_reg[NUM_VREF])(struct dim_data *, int)  = {
		NULL,
		calc_reg_v5,
		calc_reg_v15,
		calc_reg_v31,
		calc_reg_v63,
		calc_reg_v127,
		calc_reg_v191,
		calc_reg_v255,
	};

	if (br > MAX_BRIGHTNESS) {
		//printf("Warning Exceed Max brightness : %d\n", br);
		br = MAX_BRIGHTNESS;
	}

	for (i = V5; i < NUM_VREF; i++) {
#ifdef CONFIG_REF_SHIFT
		/* get reference shift value */
		shift = (int)ref_shift[br / 10][i];
#else
		shift = 0;
#endif
		gray = gamma_tbl[vref_index[i]] * br;
		index = lookup_volt_index(data, gray);
		index = index + shift;
		
		for (j = 0; j< CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				data->look_volt[i][j] = data->volt[index][j];
#ifdef DEBUG 
				printf("volt : %d : %f\n", data->look_volt[i][j], 
					(float)(data->look_volt[i][j]/1024.0));
#endif
			}
		}
	}
	for (i = V5; i < NUM_VREF; i++) { 
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				index = (i * CI_MAX) + j;
#ifdef CONFIG_COLOR_SHIFT
				c_shift = (int)color_shift[br/10][index];
#else
				c_shift = 0;
#endif
				if (c_shift & 0x80)
					gamma_int[i][j] = calc_reg[i](data, j) - (0xff - c_shift + 1);
				else
					gamma_int[i][j] = calc_reg[i](data, j) + c_shift;
#ifdef DEBUG
				printk("gamma : %x shift : %d ", gamma_int[i][j], c_shift);		
#endif
				if (gamma_int[i][j] >= vreg_element_max[i]) {
					gamma_int[i][j] = vreg_element_max[i];
#ifdef DEBUG
					printk("waring exceed\n");
#endif
				}

				if (gamma_int[i][j] < 0) {
					gamma_int[i][j] = 0;
				}
#ifdef DEBUG
				printk("\n");
#endif
			}
		}
	}
	for (j = 0; j < CI_MAX; j++)
		gamma_int[VT][j] = data->mtp_gamma[VT][j];

	index = 0;
	for (i = VT; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			if (i == V255) {
				result[index++] = gamma_int[i][j] > 0xff ? 1 : 0;
				result[index++] = gamma_int[i][j] & 0xff; 
			} else {
				result[index++] = (unsigned char)gamma_int[i][j];
			}
		}
	}


	return ret;
}


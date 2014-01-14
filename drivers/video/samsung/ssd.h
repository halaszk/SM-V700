#ifndef __SSD_H__
#define __SSD_H__

#define MAX_GRAD 	256
#define VREF_NUM	8

/* color index */
enum {
	CI_RED = 0,
	CI_GREEN,
	CI_BLUE,
	CI_MAX,
};

struct v_constant {
	int nu;
	int de;
};


struct dim_data {
	int mtp_gamma[VREF_NUM][CI_MAX];
	int volt[MAX_GRAD][CI_MAX];
	int volt_vt[CI_MAX];
	int look_volt[VREF_NUM][CI_MAX];
};


int generate_volt_table(struct dim_data *data);
int get_gamma(struct dim_data *data, int br, unsigned char *result);
int read_mtp(struct dim_data *data, const unsigned char *gamma_reg);

#endif // __SSD_H__


/*
 * board-watch-display.c - lcd driver of B Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/lcd.h>

#include <plat/devs.h>
#include <plat/fb-s5p.h>
#include <plat/gpio-cfg.h>
#include <plat/pd.h>
#include <plat/map-base.h>
#include <plat/map-s5p.h>

#ifdef CONFIG_FB_S5P_MIPI_DSIM
#include <mach/mipi_ddi.h>
#include <mach/dsim.h>
#endif



#if defined(CONFIG_FB_S5P_S6E63J0X03)
#define DSIM_NO_DATA_LANE	DSIM_DATA_LANE_1
/* 500Mbps */
#define DPHY_PLL_P	3
#define DPHY_PLL_M	125
#define DPHY_PLL_S	1

#else
#define DSIM_NO_DATA_LANE	DSIM_DATA_LANE_4
/* 500Mbps */
#define DPHY_PLL_P	3
#define DPHY_PLL_M	125
#define DPHY_PLL_S	1
#endif

unsigned int lcdtype;

static int __init lcdtype_setup(char *str)
{
	get_option(&str, &lcdtype);

	return 1;
}
__setup("lcdtype=", lcdtype_setup);


/* for Geminus based on MIPI-DSI interface */
static struct s3cfb_lcd lcd_panel_pdata_s6e8aa0 = {
	.name = "s6e8aa0",
	.height = 1280,
	.width = 720,
	.p_width = 60,		/* 59.76 mm */
	.p_height = 106,

	.bpp = 24,
	.freq = 60,

	/* minumun value is 0 except for wr_act time. */
	.cpu_timing = {
		.cs_setup = 0,
		.wr_setup = 0,
		.wr_act = 1,
		.wr_hold = 0,
	},

	.timing = {
		.h_fp = 5,
		.h_bp = 5,
		.h_sw = 5,
		.v_fp = 13,
		.v_fpe = 1,
		.v_bp = 1,
		.v_bpe = 1,
		.v_sw = 2,
		.cmd_allow_len = 11,
		.stable_vfp = 2,
	},

	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 0,
		.inv_vsync = 0,
		.inv_vden = 0,
	},
};

static void watch_enable_te(void)
{
	int reg;
	s3c_gpio_setpull(GPIO_LCD_VSYNC, S3C_GPIO_PULL_NONE);
	reg = __raw_readl(S5P_VA_GPIO2 + 0xF40);
	__raw_writel(reg, S5P_VA_GPIO2 + 0xF40);
	s3c_gpio_cfgpin(GPIO_LCD_VSYNC, S3C_GPIO_SFN(0xF));
}

static void watch_disable_te(void)
{
	int reg;
	s3c_gpio_cfgpin(GPIO_LCD_VSYNC, S3C_GPIO_SFN(0x0));
	reg = __raw_readl(S5P_VA_GPIO2 + 0xF40);
	__raw_writel(reg, S5P_VA_GPIO2 + 0xF40);
	s3c_gpio_setpull(GPIO_LCD_VSYNC, S3C_GPIO_PULL_DOWN);
}

#ifdef FIMD_OVE_TUNE
unsigned int watch_64_gamma_tbl[FIMD_GAMMA_REG_CNT] = 
{	
	0x000C0000, 0x005C002C,	0x00EC009C, 0x01B00148,
	0x02940228,	0x032402E4,	0x038C035C,	0x03DC03B8,
	0x000003FC,	0x00100000,	0x00680034,	0x00F800A8,
	0x01C00154,	0x02980230,	0x032802E8,	0x03900360,
	0x03DC03B8,	0x000003FC,	0x00100000,	0x00680034,
	0x00F800A8,	0x01C00154,	0x02980230,	0x032802E8,
	0x03900360, 0x03DC03B8,	0x000003FC,	0x00000000,
	0x00000000, 0x00000000,	0x00000000,	0x00000000,
	0x00000000,
};

#endif

/* for Geminus based on MIPI-DSI interface */
static struct s3cfb_lcd lcd_panel_pdata_s6e63j003 = {
	.name = "s6e63j0x03", 
	.height = 320,
	.width = 320,
	.p_width = 29,		/* 29.28 mm */
	.p_height = 29,
 
	.bpp = 24,
	.freq = 60,

	/* minumun value is 0 except for wr_act time. */
	.cpu_timing = {
		.cs_setup = 0,
		.wr_setup = 0,
		.wr_act = 1,
		.wr_hold = 1,
	},

	.timing = {
		.h_fp = 1,
		.h_bp = 1,
		.h_sw = 1,
		.v_fp = 500,
		.v_fpe = 1,
		.v_bp = 1,
		.v_bpe = 1,
		.v_sw = 2,
		.cmd_allow_len = 11,
		.stable_vfp = 2,
	},
	.polarity = {
		.rise_vclk = 1,
		.inv_hsync = 0,
		.inv_vsync = 0,
		.inv_vden = 0,
	},
	.enable_te = watch_enable_te,
	.disable_te = watch_disable_te,

#ifdef FIMD_OVE_TUNE
	.reg_vidcon3 = 0x00018000,
	.reg_gamma = watch_64_gamma_tbl,
	.reg_gain = 0x10040100,
#endif

};


/* define ddi platform data based on MIPI-DSI. */
static struct mipi_ddi_platform_data watch_mipi_ddi_pd = {
	.backlight_on = NULL,
};

static struct dsim_lcd_config watch_lcd_info = {
#if defined (CONFIG_FB_S5P_S6E63J0X03)
	.e_interface					= DSIM_COMMAND,
#else
	.e_interface					= DSIM_VIDEO,
#endif
	.parameter[DSI_VIRTUAL_CH_ID]	= (unsigned int) DSIM_VIRTUAL_CH_0,
	.parameter[DSI_FORMAT]		= (unsigned int) DSIM_24BPP_888,
	.parameter[DSI_VIDEO_MODE_SEL]	= (unsigned int) DSIM_NON_BURST_SYNC_EVENT,
	.mipi_ddi_pd		= (void *) &watch_mipi_ddi_pd,
#if defined(CONFIG_FB_S5P_S6E63J0X03)
	.lcd_panel_info		= (void *)&lcd_panel_pdata_s6e63j003,
#else
	.lcd_panel_info		= (void *)&lcd_panel_pdata_s6e8aa0,
#endif
};

#ifdef CONFIG_FB_SUPPORT_ALPM
extern int get_alpm_mode(void);
extern int s3c_gpio_slp_cfgpin(unsigned int pin, unsigned int config);
extern int s3c_gpio_slp_setpull_updown(unsigned int pin, unsigned int config);

#define LCD_STATUS_ON 0
#define LCD_STATUS_OFF 1
#define LCD_STATUS_ALPM_LCD_ON 2
#define LCD_STATUS_ALPM_LCD_OFF 3
#define LCD_STATUS_KNOWN 4

int alpm_status = LCD_STATUS_KNOWN;
#endif

static int reset_lcd(void)
{

	//gpio_direction_output(GPIO_MLCD_RST, 1);
	//usleep_range(1000, 1000);
	gpio_set_value(GPIO_MLCD_RST, 0);
	usleep_range(1000, 1000);
	gpio_set_value(GPIO_MLCD_RST, 1);
	return 0;
}


static int lcd_cfg_gpio(void)
{
	int err;

#ifndef CONFIG_FB_S5P_S6E63J0X03
	err = gpio_request(GPIO_LCD_22V_EN_00, "LCD_EN");
	if (err) {
		pr_err("failed to request LCD_EN gpio\n");
		return -EPERM;
	}

	s3c_gpio_cfgpin(GPIO_LCD_22V_EN_00, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_LCD_22V_EN_00, S3C_GPIO_PULL_NONE);
#endif

	/* MLCD_RST */
	err = gpio_request(GPIO_MLCD_RST, "MLCD_RST");
	if (err) {
		pr_err("failed to request MLCD_RST gpio\n");
		return -EPERM;
	}

	s3c_gpio_cfgpin(GPIO_MLCD_RST, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(GPIO_MLCD_RST, S3C_GPIO_PULL_NONE);
	gpio_set_value(GPIO_MLCD_RST, GPIO_LEVEL_HIGH);

	s3c_gpio_setpull(GPIO_LCD_VSYNC, S3C_GPIO_PULL_NONE);
	return 0;
}


static int s6e63j0x03_power_on(void)
{
	int ret = 0;
	struct regulator *regulator_18;
	struct regulator *regulator_33;

#ifndef CONFIG_FB_S5P_S6E63J0X03
	gpio_set_value(GPIO_LCD_22V_EN_00, GPIO_LEVEL_HIGH);
#endif
	regulator_18 = regulator_get(NULL, "vcc_lcd_1.8v");
	if (IS_ERR(regulator_18)) {
		pr_err("%s : failed to get regulator\n", __func__);
		goto out;
	}
	regulator_enable(regulator_18);
	regulator_put(regulator_18);

	regulator_33 = regulator_get(NULL, "vcc_3.3v_lcd");
	if (IS_ERR(regulator_33)) {
		pr_err("%s : failed to get regulator\n", __func__);
		goto out;
	}
	regulator_enable(regulator_33);
	regulator_put(regulator_33);
out:
	return ret;
}


static int s6e63j0x03_power_off(void)
{
	int ret = 0;
	struct regulator *regulator_33;
	struct regulator *regulator_18;

	gpio_set_value(GPIO_MLCD_RST, GPIO_LEVEL_LOW);

	regulator_33 = regulator_get(NULL, "vcc_3.3v_lcd");
	if (IS_ERR(regulator_33))
		goto out;
	if (regulator_is_enabled(regulator_33))
		regulator_disable(regulator_33);
	regulator_put(regulator_33);

	regulator_18 = regulator_get(NULL, "vcc_lcd_1.8v");
	if (IS_ERR(regulator_18))
		goto out;
	if (regulator_is_enabled(regulator_18))
		regulator_disable(regulator_18);
	regulator_put(regulator_18);

#ifndef CONFIG_FB_S5P_S6E63J0X03
	gpio_set_value(GPIO_LCD_22V_EN_00, GPIO_LEVEL_LOW);		
#endif

out:
	return ret;

}


static int lcd_power_on(void *ld, int enable)
{
	int ret = 0;

	pr_info("%s: enable=%d\n", __func__, enable);

#ifdef CONFIG_FB_SUPPORT_ALPM
	if (enable) {
		if (get_alpm_mode()) {
			pr_info("%s : skip lcd power on : alpm mode : on\n", __func__);
			alpm_status = LCD_STATUS_ALPM_LCD_ON;
		} else {
			pr_info("%s : panel on\n", __func__);
			if (alpm_status == LCD_STATUS_OFF) {
				pr_info("%s power on include reset\n", __func__);
				ret = s6e63j0x03_power_on();
				reset_lcd();
			} else if (alpm_status == LCD_STATUS_KNOWN) {
				pr_info("%s power on not include reset\n", __func__);
				ret = s6e63j0x03_power_on();
			}
			alpm_status = LCD_STATUS_ON;
		}

	} else {
		if (get_alpm_mode()) {
			pr_info("%s : skip lcd power off : alpm mode : on\n", __func__);
			alpm_status = LCD_STATUS_ALPM_LCD_OFF;
			/*To keep high in sleep mode when alpm on */
			s3c_gpio_slp_cfgpin(GPIO_MLCD_RST, S3C_GPIO_SLP_OUT1);
			s3c_gpio_slp_setpull_updown(GPIO_MLCD_RST, S3C_GPIO_PULL_NONE);
		} else {
			pr_info("%s : panel off\n", __func__);
			ret = s6e63j0x03_power_off();
			s3c_gpio_slp_cfgpin(GPIO_MLCD_RST, S3C_GPIO_SLP_INPUT);
			s3c_gpio_slp_setpull_updown(GPIO_MLCD_RST, S3C_GPIO_PULL_DOWN);
			alpm_status = LCD_STATUS_OFF;
		}
	}
#else
	if (enable)
		ret = s6e63j0x03_power_on();
	else
		ret = s6e63j0x03_power_off();
#endif

	return ret;
}

static void s5p_dsim_mipi_power_control(int enable)
{
	struct regulator *regulator;

	pr_info("%s: enable=%d\n", __func__, enable);

	if (enable) {
		regulator = regulator_get(NULL, "vmipi_1.0v");
		if (IS_ERR(regulator))
			goto out;
		regulator_enable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "vmipi_1.8v");
		if (IS_ERR(regulator))
			goto out;
		regulator_enable(regulator);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "vmipi_1.8v");
		if (IS_ERR(regulator))
			goto out;
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		regulator_put(regulator);

		regulator = regulator_get(NULL, "vmipi_1.0v");
		if (IS_ERR(regulator))
			goto out;
		if (regulator_is_enabled(regulator))
			regulator_disable(regulator);
		regulator_put(regulator);
	}
out:
	return;
}

struct s3c_platform_fb watch_fb_data __initdata = {
	.hw_ver		= 0x70,
	.clk_name	= "fimd",
	.nr_wins	= 5,
#ifdef CONFIG_FB_S5P_DEFAULT_WINDOW
	.default_win	= CONFIG_FB_S5P_DEFAULT_WINDOW,
#endif
	.swap		= FB_SWAP_HWORD | FB_SWAP_WORD,
#if defined(CONFIG_FB_S5P_S6E63J0X03)
	.lcd		= &lcd_panel_pdata_s6e63j003,
#else
	.lcd 		= &lcd_panel_pdata_s6e8aa0,
#endif
};

//extern void s3cfb_set_display_path(void);
void __init mipi_fb_init(void)
{
	struct s5p_platform_dsim *dsim_pd = NULL;
	struct mipi_ddi_platform_data *mipi_ddi_pd = NULL;

	/* gpio pad configuration */
	lcd_cfg_gpio();

	s3cfb_set_display_path();
	/* set platform data */
	dsim_pd = s5p_device_dsim.dev.platform_data;

	dsim_pd->platform_rev = 1;
	dsim_pd->mipi_power = s5p_dsim_mipi_power_control;
	dsim_pd->dsim_lcd_info = &watch_lcd_info;


	dsim_pd->dsim_info->e_no_data_lane = DSIM_NO_DATA_LANE;
	dsim_pd->dsim_info->p = DPHY_PLL_P;
	dsim_pd->dsim_info->m = DPHY_PLL_M;
	dsim_pd->dsim_info->s = DPHY_PLL_S;

	mipi_ddi_pd = &watch_mipi_ddi_pd;
	mipi_ddi_pd->lcd_power_on = lcd_power_on;

#if defined(CONFIG_FB_S5P_I80_LCD)
	lcd_panel_pdata_s6e63j003.te_irq = gpio_to_irq(GPIO_LCD_VSYNC);
#endif

	platform_device_register(&s5p_device_dsim);

	s3cfb_set_platdata(&watch_fb_data);	

}

#if defined(CONFIG_LCD_FREQ_SWITCH)
struct platform_device lcdfreq_device = {
		.name		= "lcdfreq",
		.id		= -1,
		.dev		= {
			.parent = &s3c_device_fb.dev,
			.platform_data = &lcd_panel_pdata.freq_limit
	},
};
#endif


/*
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/switch.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/regulator/machine.h>
#include <linux/platform_device.h>
#include <plat/adc.h>
	 
#include <mach/gpio-midas.h>
	 
#include <linux/battery/sec_battery.h>
#include <linux/battery/sec_fuelgauge.h>
#include <linux/battery/sec_charger.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <plat/gpio-cfg.h>
#include <mach/usb_switch.h>
	 
#include <linux/usb/android_composite.h>
#include <plat/devs.h>
#include <mach/regs-pmu.h>

int current_cable_type;
extern bool is_jig_attached;

#define SEC_BATTERY_PMIC_NAME ""

sec_battery_platform_data_t sec_battery_pdata;

static struct i2c_gpio_platform_data gpio_i2c_data_fgchg = {
	.sda_pin = GPIO_FUEL_SDA,
	.scl_pin = GPIO_FUEL_SCL,
};

static sec_charging_current_t charging_current_table[] = {
	{250,	250,	120,	30*60},
	{0,	0,	0,	0},
	{250,	250,	120,	30*60},
	{250,	250,	120,	30*60},
	{250,	250,	120,	30*60},
	{250,	250,	120,	30*60},
	{250,	250,	120,	30*60},
	{250,	250,	120,	30*60},
	{250,	250,	120,	30*60},
	{0,	0,	0,	0},
	{250,	250,	120,	30*60},
	{250,	250,	120,	30*60},
	{0,	0,	0,	0},
	{0,	0,	0,	0},
};

static bool sec_bat_adc_none_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_none_exit(void) {return true; }
static int sec_bat_adc_none_read(unsigned int channel) {return 0; }

static struct s3c_adc_client *adc_client;
static bool sec_bat_adc_ap_init(
		struct platform_device *pdev)
{
	adc_client = s3c_adc_register(pdev, NULL, NULL, 0);
	return true;
}

static bool sec_bat_adc_ap_exit(void) {return true; }
static int sec_bat_adc_ap_read(unsigned int channel)
{
	int data = 2500;

	if (!adc_client)
		return data;

	switch (channel) {
	case SEC_BAT_ADC_CHANNEL_TEMP:
		data = s3c_adc_read(adc_client, 2);
		break;
	case SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT:
		data = s3c_adc_read(adc_client, 1);
		break;
	case SEC_BAT_ADC_CHANNEL_VOLTAGE_NOW:
		data = 2500;/*s3c_adc_read(adc_client, 0)*/
		break;
	}

	return data;
}

static bool sec_bat_adc_ic_init(
		struct platform_device *pdev) {return true; }
static bool sec_bat_adc_ic_exit(void) {return true; }
static int sec_bat_adc_ic_read(unsigned int channel) {return 0; }

static bool sec_bat_gpio_init(void)
{
	return true;
}

static bool sec_fg_gpio_init(void)
{
	s3c_gpio_cfgpin(GPIO_FUEL_nALRT, S3C_GPIO_INPUT);
	s3c_gpio_setpull(GPIO_FUEL_nALRT, S3C_GPIO_PULL_NONE);
	return true;
}

static bool sec_chg_gpio_init(void)
{
	return true;
}

static bool sec_bat_is_lpm(void) {
	u32 lpcharging;

	lpcharging = __raw_readl(S5P_INFORM2);

	if (lpcharging == 0x01)
		return true;
	else
		return false;
}

int extended_cable_type;

static void sec_bat_initial_check(void)
{
	union power_supply_propval value;

	psy_do_property("sec-charger", get,
		POWER_SUPPLY_PROP_ONLINE, value);
	if (value.intval != current_cable_type)
		psy_do_property("battery", set,
			POWER_SUPPLY_PROP_ONLINE, value);
}

static bool sec_bat_check_jig_status(void)
{
	return is_jig_attached;
}
static bool sec_bat_switch_to_check(void) {return true; }
static bool sec_bat_switch_to_normal(void) {return true; }

static int sec_bat_check_cable_callback(void)
{
#if 0
	if(current_cable_type == POWER_SUPPLY_TYPE_BATTERY)
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
	else
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
#endif
	return current_cable_type;
}

static int sec_bat_get_cable_from_extended_cable_type(
	int input_extended_cable_type)
{
	int cable_main, cable_sub, cable_power;
	int cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
	union power_supply_propval value;
	int charge_current_max = 0, charge_current = 0;

	cable_main = GET_MAIN_CABLE_TYPE(input_extended_cable_type);
	if (cable_main != POWER_SUPPLY_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_MAIN_MASK) |
			(cable_main << ONLINE_TYPE_MAIN_SHIFT);
	cable_sub = GET_SUB_CABLE_TYPE(input_extended_cable_type);
	if (cable_sub != ONLINE_SUB_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_SUB_MASK) |
			(cable_sub << ONLINE_TYPE_SUB_SHIFT);
	cable_power = GET_POWER_CABLE_TYPE(input_extended_cable_type);
	if (cable_power != ONLINE_POWER_TYPE_UNKNOWN)
		extended_cable_type = (extended_cable_type &
			~(int)ONLINE_TYPE_PWR_MASK) |
			(cable_power << ONLINE_TYPE_PWR_SHIFT);

	switch (cable_main) {
	case POWER_SUPPLY_TYPE_CARDOCK:
		switch (cable_power) {
		case ONLINE_POWER_TYPE_BATTERY:
			cable_type = POWER_SUPPLY_TYPE_BATTERY;
			break;
		case ONLINE_POWER_TYPE_TA:
			switch (cable_sub) {
			case ONLINE_SUB_TYPE_MHL:
				cable_type = POWER_SUPPLY_TYPE_USB;
				break;
			case ONLINE_SUB_TYPE_AUDIO:
			case ONLINE_SUB_TYPE_DESK:
			case ONLINE_SUB_TYPE_SMART_NOTG:
			case ONLINE_SUB_TYPE_KBD:
				cable_type = POWER_SUPPLY_TYPE_MAINS;
				break;
			case ONLINE_SUB_TYPE_SMART_OTG:
				cable_type = POWER_SUPPLY_TYPE_CARDOCK;
				break;
			}
			break;
		case ONLINE_POWER_TYPE_USB:
			cable_type = POWER_SUPPLY_TYPE_USB;
			break;
		default:
			cable_type = current_cable_type;
			break;
		}
		break;
	case POWER_SUPPLY_TYPE_MISC:
		switch (cable_sub) {
		case ONLINE_SUB_TYPE_MHL:
			switch (cable_power) {
			case ONLINE_POWER_TYPE_BATTERY:
				cable_type = POWER_SUPPLY_TYPE_BATTERY;
				break;
			case ONLINE_POWER_TYPE_TA:
				cable_type = POWER_SUPPLY_TYPE_MISC;
				charge_current_max = 400;
				charge_current = 400;
				break;
			case ONLINE_POWER_TYPE_USB:
				cable_type = POWER_SUPPLY_TYPE_USB;
				charge_current_max = 300;
				charge_current = 300;
				break;
			default:
				cable_type = cable_main;
			}
			break;
		default:
			cable_type = cable_main;
			break;
		}
		break;
	default:
		cable_type = cable_main;
		break;
	}
	return cable_type;
}

extern bool is_cable_attached;
static bool sec_bat_check_cable_result_callback(
				int cable_type)
{
	struct usb_gadget *gadget =
		platform_get_drvdata(&s3c_device_usbgadget);


	static bool is_prev_usbtype = false;

	current_cable_type = cable_type;
	if (cable_type == POWER_SUPPLY_TYPE_BATTERY)
		is_cable_attached = false;
	else
		is_cable_attached = true;

	if (gadget) {
		pr_info("%s: cable_type=%d\n", __func__, cable_type);

		if(cable_type == POWER_SUPPLY_TYPE_USB ) {
			usb_gadget_vbus_connect(gadget);
			is_prev_usbtype = true;        
		}
		else if ( is_prev_usbtype == true) {
			usb_gadget_vbus_disconnect(gadget);
			is_prev_usbtype = false;
		}
	}
	else {
		printk(KERN_ERR "Gadget is null\n");
	}

	return true;
}

/* callback for battery check
 * return : bool
 * true - battery detected, false battery NOT detected
 */
static bool sec_bat_check_callback(void) {return true; }
static bool sec_bat_check_result_callback(void) {return true; }

/* callback for OVP/UVLO check
 * return : int
 * battery health
 */
static int sec_bat_ovp_uvlo_callback(void)
{
	int health;
	health = POWER_SUPPLY_HEALTH_GOOD;

	return health;
}

static bool sec_bat_ovp_uvlo_result_callback(int health) {return true; }

/*
 * val.intval : temperature
 */
static bool sec_bat_get_temperature_callback(
		enum power_supply_property psp,
		union power_supply_propval *val) {return true; }
static bool sec_fg_fuelalert_process(bool is_fuel_alerted) {return true; }

static const sec_bat_adc_table_data_t temp_table[] = {
	{  204,  800 },
	{  210,  790 },
	{  216,  780 },
	{  223,  770 },
	{  230,  760 },
	{  237,  750 },
	{  244,  740 },
	{  252,  730 },
	{  260,  720 },
	{  268,  710 },
	{  276,  700 },
	{  285,  690 },
	{  294,  680 },
	{  303,  670 },
	{  312,  660 },
	{  322,  650 },
	{  332,  640 },
	{  342,  630 },
	{  353,  620 },
	{  364,  610 },
	{  375,  600 },
	{  387,  590 },
	{  399,  580 },
	{  411,  570 },
	{  423,  560 },
	{  436,  550 },
	{  450,  540 },
	{  463,  530 },
	{  477,  520 },
	{  492,  510 },
	{  507,  500 },
	{  522,  490 },
	{  537,  480 },
	{  553,  470 },
	{  569,  460 },
	{  586,  450 },
	{  603,  440 },
	{  621,  430 },
	{  638,  420 },
	{  657,  410 },
	{  675,  400 },
	{  694,  390 },
	{  713,  380 },
	{  733,  370 },
	{  753,  360 },
	{  773,  350 },
	{  794,  340 },
	{  815,  330 },
	{  836,  320 },
	{  858,  310 },
	{  880,  300 },
	{  902,  290 },
	{  924,  280 },
	{  947,  270 },
	{  969,  260 },
	{  992,  250 },
	{ 1015,  240 },
	{ 1039,  230 },
	{ 1062,  220 },
	{ 1086,  210 },
	{ 1109,  200 },
	{ 1133,  190 },
	{ 1156,  180 },
	{ 1180,  170 },
	{ 1204,  160 },
	{ 1227,  150 },
	{ 1250,  140 },
	{ 1274,  130 },
	{ 1297,  120 },
	{ 1320,  110 },
	{ 1343,  100 },
	{ 1366,   90 },
	{ 1388,   80 },
	{ 1410,   70 },
	{ 1432,   60 },
	{ 1454,   50 },
	{ 1475,   40 },
	{ 1496,   30 },
	{ 1516,   20 },
	{ 1536,   10 },
	{ 1556,    0 },
	{ 1576,  -10 },
	{ 1595,  -20 },
	{ 1613,  -30 },
	{ 1631,  -40 },
	{ 1649,  -50 },
	{ 1666,  -60 },
	{ 1683,  -70 },
	{ 1699,  -80 },
	{ 1714,  -90 },
	{ 1730, -100 },
	{ 1744, -110 },
	{ 1759, -120 },
	{ 1773, -130 },
	{ 1786, -140 },
	{ 1799, -150 },
	{ 1811, -160 },
	{ 1823, -170 },
	{ 1835, -180 },
	{ 1846, -190 },
	{ 1856, -200 },
};

static const sec_bat_adc_table_data_t temp_table_for_REV04[] = {
	{  196,  800 },
	{  221,  750 },
	{  259,  700 },
	{  309,  650 },
	{  327,  630 },
	{  350,  610 },
	{  358,  600 },
	{  370,  590 },
	{  394,  570 },
	{  432,  550 },
	{  449,  530 },
	{  481,  510 },
	{  494,  500 },
	{  517,  490 },
	{  547,  470 },
	{  574,  450 },
	{  619,  430 },
	{  664,  410 },
	{  673,  400 },
	{  702,  390 },
	{  739,  370 },
	{  766,  350 },
	{  874,  300 },
	{ 1008,  250 },
	{ 1124,  200 },
	{ 1235,  150 },
	{ 1322,  100 },
	{ 1441,   50 },
	{ 1472,   40 },
	{ 1492,   30 },
	{ 1521,   20 },
	{ 1531,   10 },
	{ 1553,    0 },
	{ 1580,  -10 },
	{ 1598,  -20 },
	{ 1611,  -30 },
	{ 1623,  -40 },
	{ 1647,  -50 },
	{ 1666,  -60 },
	{ 1676,  -70 },
	{ 1696,  -80 },
	{ 1726,  -90 },
	{ 1748, -100 },
	{ 1803, -150 },
	{ 1852, -200 },
};

/* ADC region should be exclusive */
static sec_bat_adc_region_t cable_adc_value_table[] = {
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
	{0,	0},
};

static int polling_time_table[] = {
	10,	/* BASIC */
	30,	/* CHARGING */
	30,	/* DISCHARGING */
	30,	/* NOT_CHARGING */
	60 * 60,	/* SLEEP */
};

#if defined(CONFIG_FUELGAUGE_ADC)
/* soc should be 0.01% unit */
static const sec_bat_adc_table_data_t adc_ocv2soc_table[] = {
	{3400,	0},
	{3500,	400},
	{3600,	1300},
	{3700,	4200},
	{3800,	5000},
	{3900,	6000},
	{4000,	7000},
	{4100,	7900},
	{4200,	8500},
	{4350,	10000},
};

static const sec_bat_adc_table_data_t adc_adc2vcell_table[] = {
	{2150,	3400},
	{2200,	3500},
	{2250,	3600},
	{2300,	3700},
	{2350,	3800},
	{2400,	3900},
	{2450,	4000},
	{2500,	4100},
	{2550,	4200},
	{2650,	4350},
};

static struct battery_data_t watch_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
		.adc_check_count = 10,
		.monitor_polling_time = 5,
		.ocv2soc_table = adc_ocv2soc_table,
		.ocv2soc_table_size =
			sizeof(adc_ocv2soc_table) /
			sizeof(sec_bat_adc_table_data_t),
		.adc2vcell_table = adc_adc2vcell_table,
		.adc2vcell_table_size =
			sizeof(adc_adc2vcell_table) /
			sizeof(sec_bat_adc_table_data_t),
		.type_str = "SDI",
	}
};
#endif

#if defined(CONFIG_FUELGAUGE_MAX17048)
static struct battery_data_t watch_battery_data[] = {
	/* SDI battery data (High voltage 4.35V) */
	{
		.RCOMP0 = 113,
		.RCOMP_charging = 156,
		.temp_cohot = -2000,	/* x 1000 */
		.temp_cocold = -1800,	/* x 1000 */
		.is_using_model_data = false,	/* using 18 bits */
		.type_str = "SDI",
	}
};
#endif

sec_battery_platform_data_t sec_battery_pdata = {
	/* NO NEED TO BE CHANGED */
	.initial_check = sec_bat_initial_check,
	.bat_gpio_init = sec_bat_gpio_init,
	.fg_gpio_init = sec_fg_gpio_init,
	.chg_gpio_init = sec_chg_gpio_init,

	.is_lpm = sec_bat_is_lpm,
	.check_jig_status = sec_bat_check_jig_status,
	.check_cable_callback =
		sec_bat_check_cable_callback,
	.get_cable_from_extended_cable_type =
		sec_bat_get_cable_from_extended_cable_type,
	.cable_switch_check = sec_bat_switch_to_check,
	.cable_switch_normal = sec_bat_switch_to_normal,
	.check_cable_result_callback =
		sec_bat_check_cable_result_callback,
	.check_battery_callback =
		sec_bat_check_callback,
	.check_battery_result_callback =
		sec_bat_check_result_callback,
	.ovp_uvlo_callback = sec_bat_ovp_uvlo_callback,
	.ovp_uvlo_result_callback =
		sec_bat_ovp_uvlo_result_callback,
	.fuelalert_process = sec_fg_fuelalert_process,
	.get_temperature_callback =
		sec_bat_get_temperature_callback,

	.adc_api[SEC_BATTERY_ADC_TYPE_NONE] = {
		.init = sec_bat_adc_none_init,
		.exit = sec_bat_adc_none_exit,
		.read = sec_bat_adc_none_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_AP] = {
		.init = sec_bat_adc_ap_init,
		.exit = sec_bat_adc_ap_exit,
		.read = sec_bat_adc_ap_read
		},
	.adc_api[SEC_BATTERY_ADC_TYPE_IC] = {
		.init = sec_bat_adc_ic_init,
		.exit = sec_bat_adc_ic_exit,
		.read = sec_bat_adc_ic_read
		},
	.cable_adc_value = cable_adc_value_table,
	.charging_current = charging_current_table,
	.polling_time = polling_time_table,
	/* NO NEED TO BE CHANGED */

	.pmic_name = SEC_BATTERY_PMIC_NAME,

	.adc_check_count = 6,
	.adc_type = {
		SEC_BATTERY_ADC_TYPE_NONE,	/* CABLE_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* BAT_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP */
		SEC_BATTERY_ADC_TYPE_AP,	/* TEMP_AMB */
		SEC_BATTERY_ADC_TYPE_AP,	/* FULL_CHECK */
		SEC_BATTERY_ADC_TYPE_AP,	/* VOLTAGE_NOW */
	},

	/* Battery */
	.vendor = "SDI SDI",
	.technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_data = (void *)watch_battery_data,
	.bat_gpio_ta_nconnected = 0,
	.bat_polarity_ta_nconnected = 0,
	.bat_irq = 0,
	.bat_irq_attr = 0,
	.cable_check_type =
		SEC_BATTERY_CABLE_CHECK_CHGINT,
	.cable_source_type =
		SEC_BATTERY_CABLE_SOURCE_EXTERNAL,

	.event_check = true,
	.event_waiting_time = 180,

	/* Monitor setting */
	.polling_type = SEC_BATTERY_MONITOR_ALARM,
	.monitor_initial_count = 3,

	/* Battery check */
	.battery_check_type = SEC_BATTERY_CHECK_NONE,
	.check_count = 0,
	/* Battery check by ADC */
	.check_adc_max = 1440,
	.check_adc_min = 0,

	/* OVP/UVLO check */
	.ovp_uvlo_check_type = SEC_BATTERY_OVP_UVLO_CHGPOLLING,

	/* Temperature check */
	.thermal_source = SEC_BATTERY_THERMAL_SOURCE_ADC,
	.temp_adc_table = temp_table,
	.temp_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),
	.temp_amb_adc_table = temp_table,
	.temp_amb_adc_table_size =
		sizeof(temp_table)/sizeof(sec_bat_adc_table_data_t),

	.temp_check_type = SEC_BATTERY_TEMP_CHECK_TEMP,
	.temp_check_count = 1,

	.temp_high_threshold_event = 600,
	.temp_high_recovery_event = 442,
	.temp_low_threshold_event = -50,
	.temp_low_recovery_event = 0,

	.temp_high_threshold_normal = 520,
	.temp_high_recovery_normal = 442,
	.temp_low_threshold_normal = -50,
	.temp_low_recovery_normal = 0,

	.temp_high_threshold_lpm = 520,
	.temp_high_recovery_lpm = 442,
	.temp_low_threshold_lpm = -50,
	.temp_low_recovery_lpm = 0,

	.full_check_type = SEC_BATTERY_FULLCHARGED_CHGPSY,
	.full_check_type_2nd = SEC_BATTERY_FULLCHARGED_TIME,
	.full_check_count = 1,
	.chg_gpio_full_check = 0,
	.chg_polarity_full_check = 1,
	.full_condition_type = SEC_BATTERY_FULL_CONDITION_SOC |
		SEC_BATTERY_FULL_CONDITION_NOTIMEFULL |
		SEC_BATTERY_FULL_CONDITION_VCELL,
	.full_condition_soc = 97,
	.full_condition_vcell = 4300,

	.recharge_check_count = 2,
	.recharge_condition_type =
		SEC_BATTERY_RECHARGE_CONDITION_VCELL,
	.recharge_condition_soc = 90,
	.recharge_condition_vcell = 4300,

	.charging_total_time = 6 * 60 * 60,
	.recharging_total_time = 90 * 60,
	.charging_reset_time = 0,

	/* Fuel Gauge */
	.fg_irq = 0,
	.fg_irq_attr = 0,
	.fuel_alert_soc = 2,
	.repeated_fuelalert = false,
	.capacity_calculation_type =
		SEC_FUELGAUGE_CAPACITY_TYPE_RAW |
		SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
		SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE,
	.capacity_max = 1000,
	.capacity_max_margin = 50,
	.capacity_min = 10,

	/* Charger */
	.charger_name = "sec-charger",
	.chg_gpio_en = 0,
	.chg_polarity_en = 0,
	.chg_gpio_status = 0,
	.chg_polarity_status = 0,
	.chg_irq = 0,
	.chg_irq_attr = 0,
	.chg_float_voltage = 4350,
};

#define SEC_FUELGAUGE_I2C_ID	14

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_battery_pdata,
};

struct platform_device sec_device_fgchg = {
	.name = "i2c-gpio",
	.id = SEC_FUELGAUGE_I2C_ID,
	.dev.platform_data = &gpio_i2c_data_fgchg,
};

static struct i2c_board_info sec_brdinfo_fgchg[] __initdata = {
#if !defined(CONFIG_CHARGER_MAX14577)
	{
		I2C_BOARD_INFO("sec-charger",
			SEC_CHARGER_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
#endif
	{
		I2C_BOARD_INFO("sec-fuelgauge",
			SEC_FUELGAUGE_I2C_SLAVEADDR),
		.platform_data	= &sec_battery_pdata,
	},
};
	
static struct platform_device *sec_battery_devices[] __initdata = {
	&sec_device_battery,
	&sec_device_fgchg,
};

static void __init exynos_watch_charger_init_board(void)
{
	pr_info("%s: system revision - %d\n", __func__, system_rev);
#if !defined(CONFIG_CHARGER_MAX14577)
	sec_battery_pdata.chg_irq = gpio_to_irq(GPIO_CHG_INTB);
	sec_battery_pdata.chg_irq_attr = IRQF_TRIGGER_FALLING;
#endif
#if defined(CONFIG_FUELGAUGE_MAX17048)
	sec_battery_pdata.fg_irq = gpio_to_irq(GPIO_FUEL_nALRT);
	sec_battery_pdata.fg_irq_attr = IRQF_TRIGGER_RISING;
#endif
	if (system_rev >= 4) {
		sec_battery_pdata.temp_adc_table = temp_table_for_REV04;
		sec_battery_pdata.temp_adc_table_size =
			sizeof(temp_table_for_REV04) /
			sizeof(sec_bat_adc_table_data_t);
		sec_battery_pdata.temp_amb_adc_table = temp_table_for_REV04;
		sec_battery_pdata.temp_amb_adc_table_size =
			sizeof(temp_table_for_REV04) /
			sizeof(sec_bat_adc_table_data_t);
	}
}

void __init exynos_watch_charger_init(void)
{
	pr_info("%s: watch charger init\n", __func__);
	exynos_watch_charger_init_board();

	platform_add_devices(sec_battery_devices,
		ARRAY_SIZE(sec_battery_devices));

	i2c_register_board_info(SEC_FUELGAUGE_I2C_ID, sec_brdinfo_fgchg,
			ARRAY_SIZE(sec_brdinfo_fgchg));
}


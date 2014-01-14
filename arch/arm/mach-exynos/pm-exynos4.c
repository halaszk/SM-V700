/* linux/arch/arm/mach-exynos/pm-exynos4.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS4210 - Power Management support
 *
 * Based on arch/arm/mach-s3c2410/pm.c
 * Copyright (c) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/regulator/machine.h>
#include <linux/interrupt.h>

#if defined(CONFIG_PM_DEBUG_SAVE)
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#endif

#if defined(CONFIG_MACH_M0_CTC)
#include <linux/mfd/max77693.h>
#endif

#include <asm/cacheflush.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cputype.h>
#include <asm/smp_scu.h>

#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/regs-srom.h>

#include <mach/regs-irq.h>
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu.h>
#include <mach/pm-core.h>
#include <mach/pmu.h>
#include <mach/smc.h>
#include <mach/gpio.h>

void (*exynos4_sleep_gpio_table_set)(void);

#ifdef CONFIG_ARM_TRUSTZONE
#define REG_INFORM0	       (S5P_VA_SYSRAM_NS + 0x8)
#define REG_INFORM1	       (S5P_VA_SYSRAM_NS + 0xC)
#else
#define REG_INFORM0	       (S5P_INFORM0)
#define REG_INFORM1	       (S5P_INFORM1)
#endif

static struct sleep_save exynos4_enable_clk[] = {
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_LEFTBUS),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_IMAGE_4212),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_RIGHTBUS),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_PERIR_4212),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_CAM),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_TV),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_MFC),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_G3D),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_LCD0),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_FSYS),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_GPS),
	SAVE_ITEM(EXYNOS4_CLKGATE_IP_PERIL),

	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_CAM),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_TV),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_LCD0),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_FSYS),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_PERIL0),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_PERIL1),
	SAVE_ITEM(EXYNOS4_CLKSRC_MASK_TOP),

	SAVE_ITEM(EXYNOS4_CLKGATE_BUS_LEFTBUS),
	SAVE_ITEM(EXYNOS4_CLKGATE_BUS_IMAGE),
	SAVE_ITEM(EXYNOS4_CLKGATE_BUS_RIGHTBUS),
	SAVE_ITEM(EXYNOS4_CLKGATE_BUS_PERIR),
	SAVE_ITEM(EXYNOS4_CLKGATE_BUS_PERIL),
};

static struct sleep_save exynos4_set_clksrc[] = {
	{ .reg = EXYNOS4_CLKSRC_MASK_TOP			, .val = 0x00000001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_CAM			, .val = 0x11111111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_TV				, .val = 0x00000011, },
	{ .reg = EXYNOS4_CLKSRC_MASK_LCD0			, .val = 0x00001111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_MAUDIO			, .val = 0x00000001, },
	{ .reg = EXYNOS4_CLKSRC_MASK_FSYS			, .val = 0x01011111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_PERIL0			, .val = 0x00011111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_PERIL1			, .val = 0x01110111, },
	{ .reg = EXYNOS4_CLKSRC_MASK_DMC			, .val = 0x00010000, },
};

static struct sleep_save exynos4210_set_clksrc[] = {
	{ .reg = EXYNOS4_CLKSRC_MASK_LCD1			, .val = 0x00001111, },
};

#if defined(CONFIG_PM_DEBUG_SAVE)
static u32 *cmu_core, *cmu_lb, *cmu_rb, *cmu_top, *cmu_cpu, *pmu, *gpio;

#define PM_DEBUG_SAVE(reg, out, base, x, y) 				\
	do { 								\
		for (i = x; i <= y; i = i + 4)				\
			__raw_writel(__raw_readl(reg(base + i)), out + (i/4)); \
	} while(0)
#define S5P_VA_DEBUG_GPIO2(x)		(S5P_VA_GPIO2 + x)

static void exynos4_pm_debug_save(void)
{
	unsigned int i;

	pr_info( "cmu_lb:0x%08X cmu_rb:0x%08X cmu_top:0x%08X cmu_core:0x%08X\n \
			cmu_cpu:0x%08X pmu:0x%08X gpio:0x%08X\n",
			virt_to_phys((void *)cmu_lb),
			virt_to_phys((void *)cmu_rb),
			virt_to_phys((void *)cmu_top),
			virt_to_phys((void *)cmu_core),
			virt_to_phys((void *)cmu_cpu),
			virt_to_phys((void *)pmu),
			virt_to_phys((void *)gpio));

	/* CMU_LEFTBUS (0x10034200 -- 0x10036014) 12KB */
	PM_DEBUG_SAVE(EXYNOS_CLKREG, cmu_lb, 0x4000, 0x200, 0x2014);

	/*  CMU_RIGHTBUS (0x10038200 -- 0x10038A14) 4KB */
	PM_DEBUG_SAVE(EXYNOS_CLKREG, cmu_rb, 0x8000, 0x200, 0xA14);

	/*  CMU_TOP (0x1003C010 -- 0x1003CA08) 4KB */
	PM_DEBUG_SAVE(EXYNOS_CLKREG, cmu_top, 0xC000, 0x10, 0xA08);

	/*  CMU_CORE (0x10040008 -- 0x10042014) 4KB */
	PM_DEBUG_SAVE(EXYNOS_CLKREG, cmu_core, 0x10000, 0x8, 0x2014);

	/*  CMU_CPU (0x10044000 -- 0x10046014) 12KB */
	PM_DEBUG_SAVE(EXYNOS_CLKREG, cmu_cpu, 0x14000, 0x0, 0x2014);

	/*  CMU_ISP (0x10048300 -- 0x10048B10) 12KB */

	/*  PMU  */
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0, 0x24);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x100, 0x108);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x200, 0x200);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x208, 0x208);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x240, 0x240);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x400, 0x40C);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x600, 0x608);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x620, 0x620);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x628, 0x628);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x700, 0x718);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x780, 0x78C);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x800, 0x81C);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x980, 0x98C);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0xA00, 0xA00);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1000, 0x1018);

	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1050, 0x1058);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1080, 0x1080);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x10C0, 0x10C4);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1100, 0x1198);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x11A0, 0x11A4);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x11B0, 0x11B4);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x11C0, 0x11DC);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1200, 0x1204);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1220, 0x1234);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x123C, 0x1240);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1250, 0x1250);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1260, 0x1260);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1280, 0x1284);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x12C0, 0x12C0);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1300, 0x1300);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1320, 0x1320);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1340, 0x1348);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x1380, 0x13A0);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x13B0, 0x13C0);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2000, 0x2008);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2080, 0x2088);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2280, 0x2284);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2400, 0x2408);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2600, 0x2608);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2620, 0x2628);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x29A8, 0x29A8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2DC8, 0x2DC8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2E08, 0x2E08);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2E28, 0x2E28);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2E48, 0x2E48);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2E68, 0x2E68);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2E88, 0x2E88);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2EA8, 0x2EA8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2EC8, 0x2EC8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x2F48, 0x2F48);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3028, 0x3028);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3108, 0x3108);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3128, 0x3128);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3148, 0x3148);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3168, 0x3168);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3188, 0x3188);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x31A8, 0x31A8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x31E8, 0x31E8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x330C, 0x330C);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3400, 0x3404);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x341C, 0x3124);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3420, 0x3424);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x343C, 0x343C);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x361C, 0x361C);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3C00, 0x3C08);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3C20, 0x3C28);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3C40, 0x3C48);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3C60, 0x3C68);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3C80, 0x3C88);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3CA0, 0x3CA8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3CB0, 0x3CB8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3CC0, 0x3CC8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3CE0, 0x3CE8);
	PM_DEBUG_SAVE(S5P_PMUREG, pmu, 0x0, 0x3D00, 0x3D08);

	/* GPIO - GPZ */
	PM_DEBUG_SAVE(S5P_VA_DEBUG_GPIO2, gpio, 0x0, 0xC00, 0xC2C);
	PM_DEBUG_SAVE(S5P_VA_DEBUG_GPIO2, gpio, 0x0, 0xC40, 0xC4C);
	PM_DEBUG_SAVE(S5P_VA_DEBUG_GPIO2, gpio, 0x0, 0xC60, 0xC6C);
	PM_DEBUG_SAVE(S5P_VA_DEBUG_GPIO2, gpio, 0x0, 0xE00, 0xE0C);
	PM_DEBUG_SAVE(S5P_VA_DEBUG_GPIO2, gpio, 0x0, 0xE80, 0xE9C);
	PM_DEBUG_SAVE(S5P_VA_DEBUG_GPIO2, gpio, 0x0, 0xF00, 0xF0C);
	PM_DEBUG_SAVE(S5P_VA_DEBUG_GPIO2, gpio, 0x0, 0xF40, 0xF4C);
	PM_DEBUG_SAVE(S5P_VA_DEBUG_GPIO2, gpio, 0x0, 0xF80, 0xF80);
}
#endif

static struct sleep_save exynos4_core_save[] = {
	/* GIC side */
	SAVE_ITEM(S5P_VA_GIC_CPU + 0x000),
	SAVE_ITEM(S5P_VA_GIC_CPU + 0x004),
	SAVE_ITEM(S5P_VA_GIC_CPU + 0x008),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x000),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x004),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x100),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x104),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x108),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x300),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x304),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x308),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x400),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x404),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x408),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x40C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x410),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x414),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x418),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x41C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x420),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x424),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x428),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x42C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x430),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x434),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x438),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x43C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x440),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x444),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x448),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x44C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x450),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x454),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x458),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x45C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x460),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x464),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x468),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x46C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x470),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x474),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x478),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x47C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x480),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x484),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x488),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x48C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x490),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x494),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x498),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x49C),

	SAVE_ITEM(S5P_VA_GIC_DIST + 0x800),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x804),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x808),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x80C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x810),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x814),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x818),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x81C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x820),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x824),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x828),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x82C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x830),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x834),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x838),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x83C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x840),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x844),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x848),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x84C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x850),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x854),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x858),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x85C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x860),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x864),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x868),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x86C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x870),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x874),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x878),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x87C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x880),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x884),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x888),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x88C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x890),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x894),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x898),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0x89C),

	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC00),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC04),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC08),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC0C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC10),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC14),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC18),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC1C),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC20),
	SAVE_ITEM(S5P_VA_GIC_DIST + 0xC24),

	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x000),
	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x010),
	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x020),
	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x030),
	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x040),
	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x050),
	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x060),
	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x070),
	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x080),
	SAVE_ITEM(S5P_VA_COMBINER_BASE + 0x090),
};

static struct sleep_save exynos4_regs_save[] = {
	/* Common GPIO Part1 */
	SAVE_ITEM(S5P_VA_GPIO + 0x700),
	SAVE_ITEM(S5P_VA_GPIO + 0x704),
	SAVE_ITEM(S5P_VA_GPIO + 0x708),
	SAVE_ITEM(S5P_VA_GPIO + 0x70C),
	SAVE_ITEM(S5P_VA_GPIO + 0x710),
	SAVE_ITEM(S5P_VA_GPIO + 0x714),
	SAVE_ITEM(S5P_VA_GPIO + 0x718),
	SAVE_ITEM(S5P_VA_GPIO + 0x730),
	SAVE_ITEM(S5P_VA_GPIO + 0x734),
	SAVE_ITEM(S5P_VA_GPIO + 0x738),
	SAVE_ITEM(S5P_VA_GPIO + 0x73C),
	SAVE_ITEM(S5P_VA_GPIO + 0x900),
	SAVE_ITEM(S5P_VA_GPIO + 0x904),
	SAVE_ITEM(S5P_VA_GPIO + 0x908),
	SAVE_ITEM(S5P_VA_GPIO + 0x90C),
	SAVE_ITEM(S5P_VA_GPIO + 0x910),
	SAVE_ITEM(S5P_VA_GPIO + 0x914),
	SAVE_ITEM(S5P_VA_GPIO + 0x918),
	SAVE_ITEM(S5P_VA_GPIO + 0x930),
	SAVE_ITEM(S5P_VA_GPIO + 0x934),
	SAVE_ITEM(S5P_VA_GPIO + 0x938),
	SAVE_ITEM(S5P_VA_GPIO + 0x93C),
	/* Common GPIO Part2 */
	SAVE_ITEM(S5P_VA_GPIO2 + 0x708),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x70C),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x710),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x714),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x718),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x71C),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x720),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x908),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x90C),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x910),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x914),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x918),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x91C),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x920),
};

static struct sleep_save exynos4210_regs_save[] = {
	/* SROM side */
	SAVE_ITEM(S5P_SROM_BW),
	SAVE_ITEM(S5P_SROM_BC0),
	SAVE_ITEM(S5P_SROM_BC1),
	SAVE_ITEM(S5P_SROM_BC2),
	SAVE_ITEM(S5P_SROM_BC3),
	/* GPIO Part1 */
	SAVE_ITEM(S5P_VA_GPIO + 0x71C),
	SAVE_ITEM(S5P_VA_GPIO + 0x720),
	SAVE_ITEM(S5P_VA_GPIO + 0x724),
	SAVE_ITEM(S5P_VA_GPIO + 0x728),
	SAVE_ITEM(S5P_VA_GPIO + 0x72C),
	SAVE_ITEM(S5P_VA_GPIO + 0x91C),
	SAVE_ITEM(S5P_VA_GPIO + 0x920),
	SAVE_ITEM(S5P_VA_GPIO + 0x924),
	SAVE_ITEM(S5P_VA_GPIO + 0x928),
	SAVE_ITEM(S5P_VA_GPIO + 0x92C),
	/* GPIO Part2 */
	SAVE_ITEM(S5P_VA_GPIO2 + 0x700),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x704),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x900),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x904),
};


static struct sleep_save exynos4x12_regs_save[] = {
	/* SROM side */
	SAVE_ITEM(S5P_SROM_BW),
	SAVE_ITEM(S5P_SROM_BC0),
	SAVE_ITEM(S5P_SROM_BC1),
	SAVE_ITEM(S5P_SROM_BC2),
	SAVE_ITEM(S5P_SROM_BC3),
	/* GPIO Part1 */
	SAVE_ITEM(S5P_VA_GPIO + 0x740),
	SAVE_ITEM(S5P_VA_GPIO + 0x744),
	SAVE_ITEM(S5P_VA_GPIO + 0x940),
	SAVE_ITEM(S5P_VA_GPIO + 0x944),
	/* GPIO Part2 */
	SAVE_ITEM(S5P_VA_GPIO2 + 0x724),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x728),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x72C),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x730),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x734),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x924),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x928),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x92C),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x930),
	SAVE_ITEM(S5P_VA_GPIO2 + 0x934),
	/* GPIO Part3 */
	SAVE_ITEM(S5P_VA_GPIO3 + 0x700),
	SAVE_ITEM(S5P_VA_GPIO3 + 0x900),
	/* GPIO Part4 */
	SAVE_ITEM(S5P_VA_GPIO4 + 0x700),
	SAVE_ITEM(S5P_VA_GPIO4 + 0x704),
	SAVE_ITEM(S5P_VA_GPIO4 + 0x708),
	SAVE_ITEM(S5P_VA_GPIO4 + 0x70C),
	SAVE_ITEM(S5P_VA_GPIO4 + 0x710),
	SAVE_ITEM(S5P_VA_GPIO4 + 0x900),
	SAVE_ITEM(S5P_VA_GPIO4 + 0x904),
	SAVE_ITEM(S5P_VA_GPIO4 + 0x908),
	SAVE_ITEM(S5P_VA_GPIO4 + 0x90C),
	SAVE_ITEM(S5P_VA_GPIO4 + 0x910),
};

#if defined(CONFIG_CACHE_L2X0)
static struct sleep_save exynos4_l2cc_save[] = {
	SAVE_ITEM(S5P_VA_L2CC + L2X0_TAG_LATENCY_CTRL),
	SAVE_ITEM(S5P_VA_L2CC + L2X0_DATA_LATENCY_CTRL),
	SAVE_ITEM(S5P_VA_L2CC + L2X0_PREFETCH_CTRL),
	SAVE_ITEM(S5P_VA_L2CC + L2X0_POWER_CTRL),
	SAVE_ITEM(S5P_VA_L2CC + L2X0_AUX_CTRL),
};
#endif

void exynos4_cpu_suspend(void)
{
	unsigned int debug_check = 0, pass = 0xFF, timeout = 0xFF, i;

	if (soc_is_exynos4210()) {
		/* eMMC power off delay (hidden register)
		 * 0x10020988 => 0: 300msec, 1: 6msec
		 */
		__raw_writel(1, S5P_PMUREG(0x0988));
	}

	if ((!soc_is_exynos4210()) && (exynos4_is_c2c_use())) {
		unsigned int tmp;

		/* Gating CLK_IEM_APC & Enable CLK_SSS */
		tmp = __raw_readl(EXYNOS4_CLKGATE_IP_DMC);
		tmp &= ~(0x1 << 17);
		tmp |= (0x1 << 4);
		__raw_writel(tmp, EXYNOS4_CLKGATE_IP_DMC);

		/* Set MAX divider for PWI */
		tmp = __raw_readl(EXYNOS4_CLKDIV_DMC1);
		tmp |= (0xF << 8);
		__raw_writel(tmp, EXYNOS4_CLKDIV_DMC1);

		/* Set clock source for PWI */
		tmp = __raw_readl(EXYNOS4_CLKSRC_DMC);
		tmp &= ~EXYNOS4_CLKSRC_DMC_MASK;
		tmp |= ((0x6 << 16)|(0x1 << 12));
		__raw_writel(tmp, EXYNOS4_CLKSRC_DMC);
	}

	do {
		if (timeout-- == 0) {
			WARN(1, "%s: Unstable check is timeout\n", __func__);
			break;
		}

		if (pass & BIT(0)) {
			debug_check = __raw_readl(EXYNOS_CLKREG(0xC620));
			if (debug_check & BIT(20)) {
				WARN(1, "DIV_CAM1 is unstable : 0x%08X\n", debug_check);
				__raw_writel(0x11111111, EXYNOS4_CLKSRC_MASK_CAM);
				continue;
			} else
				pass &= ~BIT(0);
		}
		if (pass & BIT(1)) {
			debug_check = __raw_readl(EXYNOS_CLKREG(0xC634));
			if (debug_check & BIT(0) || debug_check & BIT(4)) {
				WARN(1, "DIV_MDNIE0 or DIV_FIMD0 is unstable : 0x%08X\n", debug_check);
				__raw_writel(0x00001111, EXYNOS4_CLKSRC_MASK_LCD0);
				continue;
			} else
				pass &= ~BIT(1);
		}
		if (pass & BIT(2)) {
			debug_check = __raw_readl(EXYNOS_CLKREG(0xC650));
			if (debug_check & BIT(0) || debug_check & BIT(4)) {
				WARN(1, "UART0 or UART1 is unstable : 0x%08X\n", debug_check);
				__raw_writel(0x00011111, EXYNOS4_CLKSRC_MASK_PERIL0);
				continue;
			} else
				pass &= ~BIT(2);
		}
		if (pass & BIT(3)) {
			debug_check = __raw_readl(EXYNOS_CLKREG(0xC654));
			if (debug_check & BIT(0) || debug_check & BIT(8)) {
				WARN(1, "SPI1 or SPI1_PRE is unstable : 0x%08X\n", debug_check);
				__raw_writel(0x01110111, EXYNOS4_CLKSRC_MASK_PERIL1);
				continue;
			} else
				pass &= ~BIT(3);
		}
		if (pass & BIT(4)) {
			debug_check = __raw_readl(EXYNOS_CLKREG(0xC680));
			if (debug_check & BIT(4)) {
				WARN(1, "CAM_BLK is unstable : 0x%08X\n", debug_check);
				__raw_writel(0x00001111, EXYNOS4_CLKSRC_MASK_LCD0);
				continue;
			} else
				pass &= ~BIT(4);
		}
	} while (pass & (BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4)));

	/* enable all IP, BUS and SRC clocks */
	s3c_pm_do_save(exynos4_enable_clk, ARRAY_SIZE(exynos4_enable_clk));
	for (i = 0; i < ARRAY_SIZE(exynos4_enable_clk); i++)
		__raw_writel(0xffffffff, exynos4_enable_clk[i].reg);

	/* change mux to XusbXTI */
	__raw_writel(0x11111111, EXYNOS4_CLKSRC_CAM);
	__raw_writel(0x00001111, EXYNOS4_CLKSRC_LCD0);
	__raw_writel(0x00011111, EXYNOS4_CLKSRC_PERIL0);
	__raw_writel(0x01110055, EXYNOS4_CLKSRC_PERIL1);

#if defined(CONFIG_PM_DEBUG_SAVE)
	exynos4_pm_debug_save();
#endif
	outer_flush_all();

#ifdef CONFIG_ARM_TRUSTZONE
	exynos_smc(SMC_CMD_SLEEP, 0, 0, 0);
#else
	/* issue the standby signal into the pm unit. */
	cpu_do_idle();
#endif
}

static int exynos4_pm_prepare(void)
{
	int ret;

#if defined(CONFIG_REGULATOR)
	ret = regulator_suspend_prepare(PM_SUSPEND_MEM);
#else
	ret = 0;
#endif

	return ret;
}

static void __maybe_unused exynos4_pm_finish(void)
{
#if defined(CONFIG_REGULATOR)
	regulator_suspend_finish();
#endif
}

static void exynos4_cpu_prepare(void)
{
	if (exynos4_sleep_gpio_table_set)
		exynos4_sleep_gpio_table_set();

	/* Set value of power down register for sleep mode */

	exynos4_sys_powerdown_conf(SYS_SLEEP);
	__raw_writel(S5P_CHECK_SLEEP, REG_INFORM1);

	/* ensure at least INFORM0 has the resume address */

	__raw_writel(virt_to_phys(s3c_cpu_resume), REG_INFORM0);

	/* Before enter central sequence mode, clock src register have to set */

#ifdef CONFIG_CACHE_L2X0
	/* Disable the full line of zero */
	disable_cache_foz();
#endif

	s3c_pm_do_restore_core(exynos4_set_clksrc, ARRAY_SIZE(exynos4_set_clksrc));

	if (soc_is_exynos4210())
		s3c_pm_do_restore_core(exynos4210_set_clksrc, ARRAY_SIZE(exynos4210_set_clksrc));
}

static unsigned int exynos4_pm_check_eint_pend(void)
{
	int i;
	u32 wakeup_int_pend, pending_eint = 0;

	for (i = 0; i < 4; i++) {
		wakeup_int_pend =
			(__raw_readl(S5P_EINT_PEND(i)) & 0xff) << (i * 8);
		pending_eint |= wakeup_int_pend & ~s3c_irqwake_eintmask;
	}

	return pending_eint;
}

static int exynos4_pm_add(struct sys_device *sysdev)
{
	pm_cpu_prep = exynos4_cpu_prepare;
	pm_cpu_sleep = exynos4_cpu_suspend;

	pm_prepare = exynos4_pm_prepare;
#ifdef CONFIG_SLP
	pm_finish = exynos4_pm_finish;
#endif

	if (soc_is_exynos4210())
		pm_check_eint_pend = exynos4_pm_check_eint_pend;

	return 0;
}

static struct sysdev_driver exynos4_pm_driver = {
	.add		= exynos4_pm_add,
};

static __init int exynos4_pm_drvinit(void)
{
	unsigned int tmp;

	s3c_pm_init();

	/* All wakeup disable */

	tmp = __raw_readl(S5P_WAKEUP_MASK);
	tmp |= ((0xFF << 8) | (0x1F << 1));
	__raw_writel(tmp, S5P_WAKEUP_MASK);

	/* Disable XXTI pad in system level normal mode */
	__raw_writel(0x0, S5P_XXTI_CONFIGURATION);

	return sysdev_driver_register(&exynos4_sysclass, &exynos4_pm_driver);
}
arch_initcall(exynos4_pm_drvinit);

static void exynos4_show_wakeup_reason_eint(void)
{
	int bit, i;
	long unsigned int ext_int_pend;
	unsigned long eint_wakeup_mask;
	bool found = 0;

	eint_wakeup_mask = __raw_readl(S5P_EINT_WAKEUP_MASK);

	for (i = 0; i <= 4; i++) {
		ext_int_pend = __raw_readl(S5P_EINT_PEND(i));

		for_each_set_bit(bit, &ext_int_pend, 8) {
			int irq = IRQ_EINT(i * 8) + bit;
			struct irq_desc *desc = irq_to_desc(irq);

			if (eint_wakeup_mask & (1 << (i * 8 + bit)))
				continue;

			if (desc && desc->action && desc->action->name)
				pr_info("Resume caused by IRQ %d, %s\n", irq,
					desc->action->name);
			else
				pr_info("Resume caused by IRQ %d\n", irq);

			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}

static void exynos4_show_wakeup_reason(void)
{
	unsigned long wakeup_stat;

	wakeup_stat = __raw_readl(S5P_WAKEUP_STAT);

	if (wakeup_stat & S5P_WAKEUP_STAT_RTCALARM)
		pr_info("Resume caused by RTC alarm\n");
	else if (wakeup_stat & S5P_WAKEUP_STAT_EINT)
		exynos4_show_wakeup_reason_eint();
	else
		pr_info("Resume caused by wakeup_stat=0x%08lx\n",
			wakeup_stat);
}

static int exynos4_pm_suspend(void)
{
	unsigned long tmp;

	if (!exynos4_is_c2c_use())
		s3c_pm_do_save(exynos4_core_save, ARRAY_SIZE(exynos4_core_save));

	s3c_pm_do_save(exynos4_regs_save, ARRAY_SIZE(exynos4_regs_save));
	if (soc_is_exynos4210())
		s3c_pm_do_save(exynos4210_regs_save,
					ARRAY_SIZE(exynos4210_regs_save));
	else
		s3c_pm_do_save(exynos4x12_regs_save,
					ARRAY_SIZE(exynos4x12_regs_save));

	s3c_pm_do_save(exynos4_l2cc_save, ARRAY_SIZE(exynos4_l2cc_save));

	/* Setting Central Sequence Register for power down mode */

#if defined(CONFIG_REGULATOR_S5M8767)
	/* Change Central sequence operation to enable memory earlier */
	__raw_writel(0x8080008b, S5P_SEQ_TRANSITION0);
	__raw_writel(0x80920081, S5P_SEQ_TRANSITION1);
	__raw_writel(0x808a0093, S5P_SEQ_TRANSITION2);

	__raw_writel(0x8080008b, S5P_SEQ_COREBLK_TRANSITION0);
	__raw_writel(0x80920081, S5P_SEQ_COREBLK_TRANSITION1);
	__raw_writel(0x808a0093, S5P_SEQ_COREBLK_TRANSITION2);
#endif

	tmp = __raw_readl(S5P_CENTRAL_SEQ_CONFIGURATION);
	tmp &= ~(S5P_CENTRAL_LOWPWR_CFG);
	__raw_writel(tmp, S5P_CENTRAL_SEQ_CONFIGURATION);

	/* When enter sleep mode, USE_DELAYED_RESET_ASSERTION have to disable */
	if (!soc_is_exynos4210())
		exynos4_reset_assert_ctrl(0);

	if (!soc_is_exynos4210()) {
		tmp = S5P_USE_STANDBY_WFI0 | S5P_USE_STANDBY_WFE0;
		__raw_writel(tmp, S5P_CENTRAL_SEQ_OPTION);

		if (exynos4_is_c2c_use()) {
			tmp = __raw_readl(S5P_WAKEUP_MASK_COREBLK);
			tmp &= ~(1 << 20);
			__raw_writel(tmp, S5P_WAKEUP_MASK_COREBLK);
			tmp = __raw_readl(S5P_CENTRAL_SEQ_CONFIGURATION_COREBLK);
			tmp &= ~S5P_CENTRAL_SEQ_COREBLK_CONF;
			__raw_writel(tmp, S5P_CENTRAL_SEQ_CONFIGURATION_COREBLK);
		}
	}

	return 0;
}

#if !defined(CONFIG_CPU_EXYNOS4210)
#define CHECK_POINT printk(KERN_DEBUG "%s:%d\n", __func__, __LINE__)
#else
#define CHECK_POINT
#endif

static void exynos4_pm_resume(void)
{
	unsigned long tmp;
#if defined(CONFIG_MACH_KONA)
	int check_pending_count = 0;
#endif
	/* If PMU failed while entering sleep mode, WFI will be
	 * ignored by PMU and then exiting cpu_do_idle().
	 * S5P_CENTRAL_LOWPWR_CFG bit will not be set automatically
	 * in this situation.
	 */
	tmp = __raw_readl(S5P_CENTRAL_SEQ_CONFIGURATION);
	if (!(tmp & S5P_CENTRAL_LOWPWR_CFG)) {
		tmp |= S5P_CENTRAL_LOWPWR_CFG;
		__raw_writel(tmp, S5P_CENTRAL_SEQ_CONFIGURATION);
		/* No need to perform below restore code */
		pr_info("%s: early_wakeup\n", __func__);
		goto early_wakeup;
	}

	/* For release retention */

	__raw_writel((1 << 28), S5P_PAD_RET_MAUDIO_OPTION);
	__raw_writel((1 << 28), S5P_PAD_RET_GPIO_OPTION);
	__raw_writel((1 << 28), S5P_PAD_RET_UART_OPTION);
	__raw_writel((1 << 28), S5P_PAD_RET_MMCA_OPTION);
	__raw_writel((1 << 28), S5P_PAD_RET_MMCB_OPTION);
	__raw_writel((1 << 28), S5P_PAD_RET_EBIA_OPTION);
	__raw_writel((1 << 28), S5P_PAD_RET_EBIB_OPTION);
	__raw_writel((1 << 28), S5P_PAD_RETENTION_GPIO_COREBLK_SYS_OPTION);

	s3c_pm_do_restore(exynos4_regs_save, ARRAY_SIZE(exynos4_regs_save));
	if (soc_is_exynos4210())
		s3c_pm_do_restore(exynos4210_regs_save,
					ARRAY_SIZE(exynos4210_regs_save));
	else
		s3c_pm_do_restore(exynos4x12_regs_save,
					ARRAY_SIZE(exynos4x12_regs_save));

#if defined(CONFIG_MACH_M0_CTC)
{
	if (max7693_muic_cp_usb_state()) {
		if (system_rev < 11) {
			gpio_direction_output(GPIO_USB_BOOT_EN, 1);
		} else if (system_rev == 11) {
			gpio_direction_output(GPIO_USB_BOOT_EN, 1);
			gpio_direction_output(GPIO_USB_BOOT_EN_REV06, 1);
		} else {
			gpio_direction_output(GPIO_USB_BOOT_EN_REV06, 1);
		}
	}
}
#endif

	CHECK_POINT;

	if (!exynos4_is_c2c_use())
		s3c_pm_do_restore_core(exynos4_core_save, ARRAY_SIZE(exynos4_core_save));
	else {
		if (!soc_is_exynos4210()) {
			/* Gating CLK_SSS */
			tmp = __raw_readl(EXYNOS4_CLKGATE_IP_DMC);
			tmp &= ~(0x1 << 4);
			__raw_writel(tmp, EXYNOS4_CLKGATE_IP_DMC);
		}
	}

	/* For the suspend-again to check the value */
	s3c_suspend_wakeup_stat = __raw_readl(S5P_WAKEUP_STAT);

	CHECK_POINT;

	if ((__raw_readl(S5P_WAKEUP_STAT) == 0) && (soc_is_exynos4212() || soc_is_exynos4412())) {
		__raw_writel(__raw_readl(S5P_EINT_PEND(0)), S5P_EINT_PEND(0));
		__raw_writel(__raw_readl(S5P_EINT_PEND(1)), S5P_EINT_PEND(1));
		__raw_writel(__raw_readl(S5P_EINT_PEND(2)), S5P_EINT_PEND(2));
		__raw_writel(__raw_readl(S5P_EINT_PEND(3)), S5P_EINT_PEND(3));
		__raw_writel(0x01010001, S5P_ARM_CORE_OPTION(0));
		__raw_writel(0x00000001, S5P_ARM_CORE_OPTION(1));
		__raw_writel(0x00000001, S5P_ARM_CORE_OPTION(2));
		__raw_writel(0x00000001, S5P_ARM_CORE_OPTION(3));

#if defined(CONFIG_MACH_KONA)
		for (check_pending_count = 0; check_pending_count < 10; ) {
			if ((__raw_readl(S5P_EINT_PEND(0)) == 0) &&
				(__raw_readl(S5P_EINT_PEND(1)) == 0) &&
				(__raw_readl(S5P_EINT_PEND(2)) == 0) &&
				(__raw_readl(S5P_EINT_PEND(3)) == 0)) {
				printk(KERN_DEBUG "[kona] try clear pending register Success [%d] times\n",++check_pending_count);
				break;
			} else {
				__raw_writel(__raw_readl(S5P_EINT_PEND(0)), S5P_EINT_PEND(0));
				__raw_writel(__raw_readl(S5P_EINT_PEND(1)), S5P_EINT_PEND(1));
				__raw_writel(__raw_readl(S5P_EINT_PEND(2)), S5P_EINT_PEND(2));
				__raw_writel(__raw_readl(S5P_EINT_PEND(3)), S5P_EINT_PEND(3));
				__raw_writel(0x01010001, S5P_ARM_CORE_OPTION(0));
				__raw_writel(0x00000001, S5P_ARM_CORE_OPTION(1));
				__raw_writel(0x00000001, S5P_ARM_CORE_OPTION(2));
				__raw_writel(0x00000001, S5P_ARM_CORE_OPTION(3));
				printk(KERN_DEBUG "[kona] try clear pending register [%d] times\n",++check_pending_count);
				if (check_pending_count == 10) {
					printk("[kona] check counter expired enter bug!\n");
					BUG();
				}
			}
		}
#endif
	}

	scu_enable(S5P_VA_SCU);

	CHECK_POINT;

#ifdef CONFIG_CACHE_L2X0
#ifdef CONFIG_ARM_TRUSTZONE
	/*
	 * Restore for Outer cache
	 */
	exynos_smc(SMC_CMD_L2X0SETUP1, exynos4_l2cc_save[0].val,
				       exynos4_l2cc_save[1].val,
				       exynos4_l2cc_save[2].val);

	CHECK_POINT;

	exynos_smc(SMC_CMD_L2X0SETUP2,
			L2X0_DYNAMIC_CLK_GATING_EN | L2X0_STNDBY_MODE_EN,
			0x7C470001, 0xC200FFFF);

	CHECK_POINT;

	exynos_smc(SMC_CMD_L2X0INVALL, 0, 0, 0);

	CHECK_POINT;

	exynos_smc(SMC_CMD_L2X0CTRL, 1, 0, 0);
#else
	s3c_pm_do_restore_core(exynos4_l2cc_save, ARRAY_SIZE(exynos4_l2cc_save));
	outer_inv_all();
	/* enable L2X0*/
	writel_relaxed(1, S5P_VA_L2CC + L2X0_CTRL);
#endif
#endif

	CHECK_POINT;

early_wakeup:
	if (!soc_is_exynos4210())
		exynos4_reset_assert_ctrl(1);

#ifdef CONFIG_CACHE_L2X0
	/* Enable the full line of zero */
	enable_cache_foz();
#endif

	CHECK_POINT;

	/* Clear Check mode */
	__raw_writel(0x0, REG_INFORM1);
	exynos4_show_wakeup_reason();

	return;
}

static struct syscore_ops exynos4_pm_syscore_ops = {
	.suspend	= exynos4_pm_suspend,
	.resume		= exynos4_pm_resume,
};

static __init int exynos4_pm_syscore_init(void)
{
	register_syscore_ops(&exynos4_pm_syscore_ops);

#if defined(CONFIG_PM_DEBUG_SAVE)
	/*  PM_DEBUG_SAVE code */
	cmu_lb = kzalloc(1024 * 12, GFP_KERNEL);
	if (!cmu_lb)
		printk("%s: debug:cmu_lb is null\n", __func__);

	cmu_rb = kzalloc(1024 * 4, GFP_KERNEL);
	if (!cmu_rb)
		printk("%s: debug:cmu_rb is null\n", __func__);

	cmu_top = kzalloc(1024 * 4, GFP_KERNEL);
	if (!cmu_top)
		printk("%s: debug:cmu_top is null\n", __func__);

	cmu_core = kzalloc(1024 * 12, GFP_KERNEL);
	if (!cmu_core)
		printk("%s: debug:cmu_core is null\n", __func__);

	cmu_cpu = kzalloc(1024 * 12, GFP_KERNEL);
	if (!cmu_cpu)
		printk("%s: debug:cmu_cpu is null\n", __func__);

	pmu = kzalloc(1024 * 16, GFP_KERNEL);
	if (!pmu)
		printk("%s: debug:pmu is null\n", __func__);

	gpio = kzalloc(1024 * 4, GFP_KERNEL);
	if (!gpio)
		printk("%s: debug:pmu is null\n", __func__);
#endif
	return 0;
}
arch_initcall(exynos4_pm_syscore_init);

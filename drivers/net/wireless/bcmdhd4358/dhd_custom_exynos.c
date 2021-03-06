/*
 * Platform Dependent file for Samsung Exynos
 *
 * Copyright (C) 1999-2014, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_custom_exynos.c 500926 2014-09-05 14:59:02Z $
 */
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/io.h>
#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/bug.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/wlan_plat.h>

#include <mach/gpio.h>
#include <mach/irqs.h>
#include <linux/sec_sysfs.h>

#include <plat/gpio-cfg.h>

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM

#define WLAN_STATIC_SCAN_BUF0		5
#define WLAN_STATIC_SCAN_BUF1		6
#define WLAN_STATIC_DHD_INFO_BUF	7
#define WLAN_STATIC_DHD_WLFC_BUF        8
#define WLAN_STATIC_DHD_IF_FLOW_LKUP    9
#define WLAN_STATIC_DHD_FLOWRING	10
#define WLAN_STATIC_DHD_MEMDUMP_BUF	11
#define WLAN_STATIC_DHD_MEMDUMP_RAM	12

#define WLAN_SCAN_BUF_SIZE		(64 * 1024)
#define WLAN_DHD_INFO_BUF_SIZE		(16 * 1024)
#define WLAN_DHD_WLFC_BUF_SIZE          (16 * 1024)
#define WLAN_DHD_IF_FLOW_LKUP_SIZE      (20 * 1024)
#define WLAN_DHD_MEMDUMP_SIZE		(800 * 1024)

#define PREALLOC_WLAN_SEC_NUM		4
#define PREALLOC_WLAN_BUF_NUM		160
#define PREALLOC_WLAN_SECTION_HEADER	24

#define DHD_SKB_HDRSIZE			336
#define DHD_SKB_1PAGE_BUFSIZE	((PAGE_SIZE*1)-DHD_SKB_HDRSIZE)
#define DHD_SKB_2PAGE_BUFSIZE	((PAGE_SIZE*2)-DHD_SKB_HDRSIZE)
#define DHD_SKB_4PAGE_BUFSIZE	((PAGE_SIZE*4)-DHD_SKB_HDRSIZE)

#ifdef CONFIG_BCMDHD_PCIE
#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	0
#define WLAN_SECTION_SIZE_2	0
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_1PAGE_RESERVED_BUF_NUM	4
#define DHD_SKB_1PAGE_BUF_NUM	((32) + (DHD_SKB_1PAGE_RESERVED_BUF_NUM))
#define DHD_SKB_2PAGE_BUF_NUM	0
#define DHD_SKB_4PAGE_BUF_NUM	0

#else

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_BUF_NUM * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_BUF_NUM * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_BUF_NUM * 1024)

#define DHD_SKB_1PAGE_BUF_NUM	8
#define DHD_SKB_2PAGE_BUF_NUM	8
#define DHD_SKB_4PAGE_BUF_NUM	1
#endif /* CONFIG_BCMDHD_PCIE */

#define WLAN_SKB_1_2PAGE_BUF_NUM	((DHD_SKB_1PAGE_BUF_NUM) + \
	(DHD_SKB_2PAGE_BUF_NUM))
#define WLAN_SKB_BUF_NUM	((WLAN_SKB_1_2PAGE_BUF_NUM) + \
	(DHD_SKB_4PAGE_BUF_NUM))

#define PREALLOC_TX_FLOWS		40
#define PREALLOC_COMMON_MSGRINGS	2
#define WLAN_FLOWRING_NUM \
	((PREALLOC_TX_FLOWS) + (PREALLOC_COMMON_MSGRINGS))
#define WLAN_DHD_FLOWRING_SIZE	((PAGE_SIZE) * (7))
#ifdef CONFIG_BCMDHD_PCIE
static void *wlan_static_flowring[WLAN_FLOWRING_NUM];
#else
void *wlan_static_flowring = NULL;
#endif /* CONFIG_BCMDHD_PCIE */

#if defined(CONFIG_ARGOS)
extern int argos_irq_affinity_setup_label(unsigned int irq, const char *label,
                 struct cpumask *affinity_cpu_mask,
                 struct cpumask *default_cpu_mask);
#endif /* CONFIG_ARGOS */

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

struct wlan_mem_prealloc {
	void *mem_ptr;
	unsigned long size;
};

static struct wlan_mem_prealloc wlan_mem_array[PREALLOC_WLAN_SEC_NUM] = {
	{NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER)},
	{NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER)}
};

void *wlan_static_scan_buf0 = NULL;
void *wlan_static_scan_buf1 = NULL;
void *wlan_static_dhd_info_buf = NULL;
void *wlan_static_dhd_wlfc_buf = NULL;
void *wlan_static_if_flow_lkup = NULL;
void *wlan_static_dhd_memdump_buf = NULL;
void *wlan_static_dhd_memdump_ram = NULL;

static void *dhd_wlan_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_SEC_NUM)
		return wlan_static_skb;

	if (section == WLAN_STATIC_SCAN_BUF0)
		return wlan_static_scan_buf0;

	if (section == WLAN_STATIC_SCAN_BUF1)
		return wlan_static_scan_buf1;

	if (section == WLAN_STATIC_DHD_INFO_BUF) {
		if (size > WLAN_DHD_INFO_BUF_SIZE) {
			pr_err("request DHD_INFO size(%lu) is bigger than"
				" static size(%d).\n", size,
				WLAN_DHD_INFO_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_info_buf;
	}

	if (section == WLAN_STATIC_DHD_WLFC_BUF)  {
		if (size > WLAN_DHD_WLFC_BUF_SIZE) {
			pr_err("request DHD_WLFC size(%lu) is bigger than"
				" static size(%d).\n",
				size, WLAN_DHD_WLFC_BUF_SIZE);
			return NULL;
		}
		return wlan_static_dhd_wlfc_buf;
	}

	if (section == WLAN_STATIC_DHD_IF_FLOW_LKUP)  {
		if (size > WLAN_DHD_IF_FLOW_LKUP_SIZE) {
			pr_err("request DHD_WLFC size(%lu) is bigger than"
				" static size(%d).\n",
				size, WLAN_DHD_WLFC_BUF_SIZE);
			return NULL;
		}
		return wlan_static_if_flow_lkup;
	}

	if (section == WLAN_STATIC_DHD_FLOWRING)
		return wlan_static_flowring;

	if (section == WLAN_STATIC_DHD_MEMDUMP_BUF) {
		if (size > WLAN_DHD_MEMDUMP_SIZE) {
			pr_err("request DHD_MEMDUMP_BUF size(%lu) is bigger"
				" than static size(%d).\n",
				size, WLAN_DHD_MEMDUMP_SIZE);
			return NULL;
		}
		return wlan_static_dhd_memdump_buf;
	}

	if (section == WLAN_STATIC_DHD_MEMDUMP_RAM) {
		if (size > WLAN_DHD_MEMDUMP_SIZE) {
			pr_err("request DHD_MEMDUMP_RAM size(%lu) is bigger"
				" than static size(%d).\n",
				size, WLAN_DHD_MEMDUMP_SIZE);
			return NULL;
		}
		return wlan_static_dhd_memdump_ram;
	}

	if ((section < 0) || (section > PREALLOC_WLAN_SEC_NUM))
		return NULL;

	if (wlan_mem_array[section].size < size)
		return NULL;

	return wlan_mem_array[section].mem_ptr;
}

static int dhd_init_wlan_mem(void)
{
	int i;
	int j;

	for (i = 0; i < DHD_SKB_1PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_1PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

#if !defined(CONFIG_BCMDHD_PCIE)
	for (i = DHD_SKB_1PAGE_BUF_NUM; i < WLAN_SKB_1_2PAGE_BUF_NUM; i++) {
		wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_2PAGE_BUFSIZE);
		if (!wlan_static_skb[i])
			goto err_skb_alloc;
	}

	wlan_static_skb[i] = dev_alloc_skb(DHD_SKB_4PAGE_BUFSIZE);
	if (!wlan_static_skb[i])
		goto err_skb_alloc;
#endif /* !CONFIG_BCMDHD_PCIE */

	for (i = 0; i < PREALLOC_WLAN_SEC_NUM; i++) {
		if (wlan_mem_array[i].size > 0) {
			wlan_mem_array[i].mem_ptr =
				kmalloc(wlan_mem_array[i].size, GFP_KERNEL);

			if (!wlan_mem_array[i].mem_ptr)
				goto err_mem_alloc;
		}
	}

	wlan_static_scan_buf0 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf0) {
		pr_err("Failed to alloc wlan_static_scan_buf0\n");
		goto err_mem_alloc;
	}

	wlan_static_scan_buf1 = kmalloc(WLAN_SCAN_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_scan_buf1) {
		pr_err("Failed to alloc wlan_static_scan_buf1\n");
		goto err_mem_alloc;
	}

	wlan_static_dhd_info_buf = kmalloc(WLAN_DHD_INFO_BUF_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_info_buf) {
		pr_err("Failed to alloc wlan_static_dhd_info_buf\n");
		goto err_mem_alloc;
	}

#ifdef CONFIG_BCMDHD_PCIE
	wlan_static_if_flow_lkup = kmalloc(WLAN_DHD_IF_FLOW_LKUP_SIZE,
		GFP_KERNEL);
	if (!wlan_static_if_flow_lkup) {
		pr_err("Failed to alloc wlan_static_if_flow_lkup\n");
		goto err_mem_alloc;
	}

	memset(wlan_static_flowring, 0, sizeof(wlan_static_flowring));
	for (j = 0; j < WLAN_FLOWRING_NUM; j++) {
		wlan_static_flowring[j] =
			kmalloc(WLAN_DHD_FLOWRING_SIZE, GFP_KERNEL | __GFP_ZERO);

		if (!wlan_static_flowring[j])
			goto err_mem_alloc;
	}
#else
	wlan_static_dhd_wlfc_buf = kmalloc(WLAN_DHD_WLFC_BUF_SIZE,
		GFP_KERNEL);
	if (!wlan_static_dhd_wlfc_buf) {
		pr_err("Failed to alloc wlan_static_dhd_wlfc_buf\n");
		goto err_mem_alloc;
	}
#endif /* CONFIG_BCMDHD_PCIE */

#ifdef CONFIG_BCMDHD_DEBUG_PAGEALLOC
	wlan_static_dhd_memdump_buf = kmalloc(WLAN_DHD_MEMDUMP_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_memdump_buf) {
		pr_err("Failed to alloc wlan_static_dhd_memdump_buf\n");
		goto err_mem_alloc;
	}

	wlan_static_dhd_memdump_ram = kmalloc(WLAN_DHD_MEMDUMP_SIZE, GFP_KERNEL);
	if (!wlan_static_dhd_memdump_ram) {
		pr_err("Failed to alloc wlan_static_dhd_memdump_ram\n");
		goto err_mem_alloc;
	}
#endif /* CONFIG_BCMDHD_DEBUG_PAGEALLOC */

	pr_err("%s: WIFI MEM Allocated\n", __FUNCTION__);
	return 0;

err_mem_alloc:
#ifdef CONFIG_BCMDHD_DEBUG_PAGEALLOC
	if (wlan_static_dhd_memdump_ram)
		kfree(wlan_static_dhd_memdump_ram);

	if (wlan_static_dhd_memdump_buf)
		kfree(wlan_static_dhd_memdump_buf);
#endif /* CONFIG_BCMDHD_DEBUG_PAGEALLOC */

#ifdef CONFIG_BCMDHD_PCIE
	for (j = 0; j < WLAN_FLOWRING_NUM; j++) {
		if (wlan_static_flowring[j])
			kfree(wlan_static_flowring[j]);
	}

	if (wlan_static_if_flow_lkup)
		kfree(wlan_static_if_flow_lkup);
#else
	if (wlan_static_dhd_wlfc_buf)
		kfree(wlan_static_dhd_wlfc_buf);
#endif /* CONFIG_BCMDHD_PCIE */
	if (wlan_static_dhd_info_buf)
		kfree(wlan_static_dhd_info_buf);

	if (wlan_static_scan_buf1)
		kfree(wlan_static_scan_buf1);

	if (wlan_static_scan_buf0)
		kfree(wlan_static_scan_buf0);

	pr_err("Failed to mem_alloc for WLAN\n");

	for (j = 0; j < i; j++)
		kfree(wlan_mem_array[j].mem_ptr);

	i = WLAN_SKB_BUF_NUM;

err_skb_alloc:
	pr_err("Failed to skb_alloc for WLAN\n");
	for (j = 0; j < i; j++)
		dev_kfree_skb(wlan_static_skb[j]);

	return -ENOMEM;
}
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

#define WIFI_TURNON_DELAY	200
static struct device *wlan_dev;
static int wlan_pwr_on = -1;
int wlan_host_wake_irq = 0;
EXPORT_SYMBOL(wlan_host_wake_irq);

#ifdef CONFIG_MACH_UNIVERSAL5433
extern void exynos_pcie_poweron(void);
extern void exynos_pcie_poweroff(void);
extern int check_rev(void);
#endif /* CONFIG_MACH_UNIVERSAL5433 */

static int dhd_wlan_power(int onoff)
{
	printk(KERN_INFO"------------------------------------------------");
	printk(KERN_INFO"------------------------------------------------\n");
	printk(KERN_INFO"%s Enter: power %s\n", __FUNCTION__, onoff ? "on" : "off");

#ifdef CONFIG_MACH_UNIVERSAL5433
	if (!onoff)
		exynos_pcie_poweroff();

	/* Old revision chip can't control WL_REG_ON */
	if (check_rev()) {
		if (gpio_direction_output(wlan_pwr_on, onoff)) {
			printk(KERN_ERR "%s failed to control WLAN_REG_ON to %s\n",
				__FUNCTION__, onoff ? "HIGH" : "LOW");
			return -EIO;
		}
	}

	if (onoff)
		exynos_pcie_poweron();
#else
	if (gpio_direction_output(wlan_pwr_on, onoff)) {
		printk(KERN_ERR "%s failed to control WLAN_REG_ON to %s\n",
			__FUNCTION__, onoff ? "HIGH" : "LOW");
		return -EIO;
#endif /* CONFIG_MACH_UNIVERSAL5433 */

	return 0;
}

static int dhd_wlan_reset(int onoff)
{
	return 0;
}

#ifndef CONFIG_BCMDHD_PCIE
extern void (*notify_func_callback)(void *dev_id, int state);
extern void *mmc_host_dev;

static int dhd_wlan_set_carddetect(int val)
{
	pr_err("%s: notify_func=%p, mmc_host_dev=%p, val=%d\n",
		__FUNCTION__, notify_func_callback, mmc_host_dev, val);

	if (notify_func_callback)
		notify_func_callback(mmc_host_dev, val);
	else
		pr_warning("%s: Nobody to notify\n", __FUNCTION__);

	return 0;
}
#endif /* !CONFIG_BCMDHD_PCIE */

int __init dhd_wlan_init_gpio(void)
{
	const char *wlan_node = "samsung,brcm-wlan";
	unsigned int wlan_host_wake_up = -1;
	struct device_node *root_node = NULL;

	wlan_dev = sec_device_create(NULL, "wlan");
	BUG_ON(!wlan_dev);

	root_node = of_find_compatible_node(NULL, NULL, wlan_node);
	if (!root_node) {
		WARN(1, "failed to get device node of bcm4354\n");
		return -ENODEV;
	}

	/* ========== WLAN_PWR_EN ============ */
	wlan_pwr_on = of_get_gpio(root_node, 0);
	if (!gpio_is_valid(wlan_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_pwr_on);
		return -ENODEV;
	}

	if (gpio_request(wlan_pwr_on, "WLAN_REG_ON")) {
		WARN(1, "fail to request gpio(WLAN_REG_ON)\n");
		return -ENODEV;
	}
#ifdef CONFIG_BCMDHD_PCIE
	gpio_direction_output(wlan_pwr_on, 1);
#else
	gpio_direction_output(wlan_pwr_on, 0);
#endif /* CONFIG_BCMDHD_PCIE */
	gpio_export(wlan_pwr_on, 1);
	gpio_export_link(wlan_dev, "WLAN_REG_ON", wlan_pwr_on);
	msleep(WIFI_TURNON_DELAY);

	/* ========== WLAN_HOST_WAKE ============ */
	wlan_host_wake_up = of_get_gpio(root_node, 1);
	if (!gpio_is_valid(wlan_host_wake_up)) {
		WARN(1, "Invalied gpio pin : %d\n", wlan_host_wake_up);
		return -ENODEV;
	}

	if (gpio_request(wlan_host_wake_up, "WLAN_HOST_WAKE")) {
		WARN(1, "fail to request gpio(WLAN_HOST_WAKE)\n");
		return -ENODEV;
	}
	gpio_direction_input(wlan_host_wake_up);
	gpio_export(wlan_host_wake_up, 1);
	gpio_export_link(wlan_dev, "WLAN_HOST_WAKE", wlan_host_wake_up);

	wlan_host_wake_irq = gpio_to_irq(wlan_host_wake_up);

	return 0;
}

#if defined(CONFIG_ARGOS)
void set_cpucore_for_interrupt(cpumask_var_t default_cpu_mask,
	cpumask_var_t affinity_cpu_mask) {
#if defined(CONFIG_MACH_UNIVERSAL5433)
	argos_irq_affinity_setup_label(277, "WIFI", affinity_cpu_mask, default_cpu_mask);
#endif /* CONFIG_MACH_UNIVERSAL5433 */
}
#endif /* CONFIG_ARGOS */

void interrupt_set_cpucore(int set)
{
	printk(KERN_INFO "%s: set: %d\n", __FUNCTION__, set);
	if (set)
	{
#if defined(CONFIG_MACH_UNIVERSAL5422)
		irq_set_affinity(EXYNOS5_IRQ_HSMMC1, cpumask_of(DPC_CPUCORE));
		irq_set_affinity(EXYNOS_IRQ_EINT16_31, cpumask_of(DPC_CPUCORE));
#endif /* CONFIG_MACH_UNIVERSAL5422 */
#if defined(CONFIG_MACH_UNIVERSAL5430)
		irq_set_affinity(IRQ_SPI(226), cpumask_of(DPC_CPUCORE));
		irq_set_affinity(IRQ_SPI(2), cpumask_of(DPC_CPUCORE));
#endif /* CONFIG_MACH_UNIVERSAL5430 */
	} else {
#if defined(CONFIG_MACH_UNIVERSAL5422)
		irq_set_affinity(EXYNOS5_IRQ_HSMMC1, cpumask_of(PRIMARY_CPUCORE));
		irq_set_affinity(EXYNOS_IRQ_EINT16_31, cpumask_of(PRIMARY_CPUCORE));
#endif /* CONFIG_MACH_UNIVERSAL5422 */
#if defined(CONFIG_MACH_UNIVERSAL5430)
		irq_set_affinity(IRQ_SPI(226), cpumask_of(PRIMARY_CPUCORE));
		irq_set_affinity(IRQ_SPI(2), cpumask_of(PRIMARY_CPUCORE));
#endif /* CONFIG_MACH_UNIVERSAL5430 */
	}
}

struct resource dhd_wlan_resources = {
	.name	= "bcmdhd_wlan_irq",
	.start	= 0,
	.end	= 0,
	.flags	= IORESOURCE_IRQ | IORESOURCE_IRQ_SHAREABLE |
#ifdef CONFIG_BCMDHD_PCIE
	IORESOURCE_IRQ_HIGHEDGE,
#else
	IORESOURCE_IRQ_HIGHLEVEL,
#endif /* CONFIG_BCMDHD_PCIE */
};
EXPORT_SYMBOL(dhd_wlan_resources);

struct wifi_platform_data dhd_wlan_control = {
	.set_power	= dhd_wlan_power,
	.set_reset	= dhd_wlan_reset,
#ifndef CONFIG_BCMDHD_PCIE
	.set_carddetect	= dhd_wlan_set_carddetect,
#endif /* !CONFIG_BCMDHD_PCIE */
#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	.mem_prealloc	= dhd_wlan_mem_prealloc,
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */
};
EXPORT_SYMBOL(dhd_wlan_control);

int __init dhd_wlan_init(void)
{
	int ret;

	printk(KERN_INFO "%s: start\n", __FUNCTION__);
	ret = dhd_wlan_init_gpio();
	if (ret < 0) {
		printk(KERN_ERR "%s: failed to initiate GPIO, ret=%d\n",
			__FUNCTION__, ret);
		return ret;
	}

	dhd_wlan_resources.start = wlan_host_wake_irq;
	dhd_wlan_resources.end = wlan_host_wake_irq;

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
	dhd_init_wlan_mem();
#endif /* CONFIG_BROADCOM_WIFI_RESERVED_MEM */

	return ret;
}

device_initcall(dhd_wlan_init);

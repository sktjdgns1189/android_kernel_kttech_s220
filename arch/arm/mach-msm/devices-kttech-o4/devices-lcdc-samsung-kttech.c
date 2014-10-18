/* Copyright (c) 2010-2011, KT Tech Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#define _GET_REGULATOR(var, name) do {					\
	if (var == NULL) {						\
		var = regulator_get(NULL, name);			\
		if (IS_ERR(var)) {					\
			pr_err("'%s' regulator not found, rc=%ld\n",	\
				name, PTR_ERR(var));			\
			var = NULL;					\
		}							\
	}								\
} while (0)

#define GPIO_RESX_N   (102)

static int display_power_on;
static void display_common_power(int on)
{
	int rc;

	static struct regulator *display_reg;

	if (on) {
#if 0//def CONFIG_PMIC8901_REGISTER_TRACE
		_GET_REGULATOR(display_reg, "8901_l2l");
#else
		_GET_REGULATOR(display_reg, "8901_l2");
#endif
		if (!display_reg) {
			pr_err("%s: Unable to get 8901_l2\n", __func__);
			return;
		}
		rc = gpio_tlmm_config(GPIO_CFG(GPIO_RESX_N, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("%s: Unable to get GPIO_RESX_N \n", __func__);			
			regulator_put(display_reg);
			return;
		}
		regulator_set_voltage(display_reg, 2850000, 2850000);
		regulator_enable(display_reg);
		msleep(50);
		gpio_set_value(GPIO_RESX_N, 0);
		msleep(1);
		gpio_set_value(GPIO_RESX_N, 1);
		msleep(50);
		display_power_on = 1;
	} else {
		if (display_power_on) {	
			display_power_on = 0;
			gpio_set_value(GPIO_RESX_N, 0);
			msleep(20);
			regulator_disable(display_reg);
			regulator_put(display_reg);
			display_reg = NULL;
		}
	}
}
#undef _GET_REGULATOR

#define LCDC_NUM_GPIO 28
#define LCDC_GPIO_START 0

static void lcdc_samsung_panel_power(int on)
{
	int n, ret = 0;

	display_common_power(on);

	for (n = 0; n < LCDC_NUM_GPIO; n++) {
		if (on) {
			ret = gpio_request(LCDC_GPIO_START + n, "LCDC_GPIO");
			if (unlikely(ret)) {
				pr_err("%s not able to get gpio\n", __func__);
				break;
			}
		} else
			gpio_free(LCDC_GPIO_START + n);
	}

	if (ret) {
		for (n--; n >= 0; n--)
			gpio_free(LCDC_GPIO_START + n);
	}
}

static int lcdc_panel_power(int on)
{
	int flag_on = !!on;
	static int lcdc_power_save_on;

	if (lcdc_power_save_on == flag_on)
		return 0;

	lcdc_power_save_on = flag_on;

	lcdc_samsung_panel_power(on);

	return 0;
}

static struct lcdc_platform_data lcdc_pdata = {
	.lcdc_power_save   = lcdc_panel_power,
};


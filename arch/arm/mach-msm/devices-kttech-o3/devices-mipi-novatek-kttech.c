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

#define GPIO_LCD_RESET_N	102	/* LCD Reset */
#define MDP_VSYNC_GPIO		28

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

static int retry_val = 0;

static int mipi_dsi_panel_power(int on){
	int rc = 0;
	int flags_on = !!on;
	static int mipi_dsi_power_save_on;
	static struct regulator *display_reg;		/* LCD VDD */
	static struct regulator *reg_8901_lvs2;		/* PM8901 LVS2 */

//	DEBUG_KTTECH_INFO("LCD Power Func(+) : %s, ON,OFF : %d\n", __func__, on);

	if (mipi_dsi_power_save_on == flags_on) {
			pr_err("%s: Already Power Saved DSI.\n", __func__);	
			return 0;
	}

	mipi_dsi_power_save_on = flags_on;	

retry:
	if (reg_8901_lvs2 == NULL)
		_GET_REGULATOR(reg_8901_lvs2, "8901_lvs2");  

        if (reg_8901_lvs2 == NULL) {
                pr_err("%s: Regulator Get (8901_LVS2, 1.8v) Failed!\n", __func__);
                return 0;
        }

	if (display_reg == NULL)
		_GET_REGULATOR(display_reg, "8901_l2");

	if (display_reg == NULL) {
		pr_err("%s: Regulator Get (8902_L2, 3.0v) Failed!\n", __func__);
		return 0; 
	}

	rc = gpio_tlmm_config(GPIO_CFG(GPIO_LCD_RESET_N, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	if (rc) {
		pr_err("%s: LCD_RESET_N gpio %d request or tlmm_config"
			"failed\n", __func__,
			GPIO_LCD_RESET_N);
		goto out;
		}

	if (flags_on) {
		rc = regulator_set_voltage(display_reg, 3000000, 3000000);
		if (rc) {
			pr_err("%s: Regulator Set Voltave (8901_L2, 3.0v) Failed!\n", __func__);
			goto out;
		}

		rc = regulator_enable(display_reg);
		if (rc) {
			pr_err("%s: Regulator Enable (8901_L2, 3.0v) Failed!\n", __func__);	
			goto out;
		}

		msleep(5);	

                rc = regulator_enable(reg_8901_lvs2);
                if (rc) {
                        pr_err("%s: Regulator Enable (8901_LVS2, 1.8v) Failed!\n", __func__);
                        goto out;
                }
	
		msleep(10);

		/* LCD Reset Sequence */
		gpio_set_value(GPIO_LCD_RESET_N, 1);
		msleep(1);
		gpio_set_value(GPIO_LCD_RESET_N, 0);
		msleep(1);
		gpio_set_value(GPIO_LCD_RESET_N, 1);
		msleep(50);
	} 
	else {
			gpio_set_value(GPIO_LCD_RESET_N, 0);
			msleep(1);
			rc = regulator_disable(display_reg);
			if (rc) {
				pr_err("%s: Regulator Disable (8901_L2, 3.0v) Failed!\n", __func__);
				goto out;
			}
			msleep(1);

			rc = regulator_disable(reg_8901_lvs2);
		
			if (rc) {
				pr_err("%s: Regulator Disable (8901_LVS2, 1.8v) Failed!\n", __func__);
			}
			msleep(1);
	}
	return 0;

out:
	retry_val++;
	pr_err("%s: A Problem Occured! on MIPI-DSI Power Control."
		"Need to Re-initialize DSI!, Retry : %d\n", __func__, retry_val);
	regulator_put(reg_8901_lvs2);
	reg_8901_lvs2 = NULL;
	regulator_put(display_reg);
	display_reg = NULL;		
	gpio_set_value(GPIO_LCD_RESET_N, 0);

	if(retry_val < 5) { 
		goto retry;
	}
	else {
		pr_err("%s: A Problem Occured! on MIPI-DSI Power Control."
			"Re-initialize DSI!, Failed!\n", __func__);
		return rc;
	}
}

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio = MDP_VSYNC_GPIO,
	.dsi_power_save   = mipi_dsi_panel_power,
};


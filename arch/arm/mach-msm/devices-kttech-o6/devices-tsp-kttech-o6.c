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
 
/* ATMEL MXT224 TSP */
#ifdef CONFIG_KTTECH_TOUCH_QT602240
#include <linux/qt602240-kttech_v2.h>
#endif
/* ATMEL MXT224 TSP, Driver ver.3 */
#ifdef CONFIG_KTTECH_TOUCH_QT602240V3
#include <linux/qt602240-kttech_v3.h>
#endif
/* ATMEL MXT224E TSP */
#ifdef CONFIG_KTTECH_TOUCH_QT602240V3E
#include <linux/qt602240-kttech_v3e.h>
#endif
/* MELFAS MCS8000 TSP */
#ifdef CONFIG_KTTECH_TOUCH_MCS8000
#include <linux/mcs8000_ts-kttech.h>
#endif

/* For MXT224 ver.2 drivers */
#ifdef CONFIG_KTTECH_TOUCH_QT602240
/* TSP */static struct qt602240_platform_data qt602240_platform_data = {	
	.x_line    = 19,
	.y_line    = 11,
	.x_size    = 1024, /* (960 = LCD, 64 = touch) */
	.y_size    = 540,
	.blen      = 0x00,
	.threshold = 0x20,
	.irq_gpio  = 66,
	.reset_gpio = 33,
	.voltage   = 2700000, /* 2.7V */  
	.orient    = QT602240_DIAGONAL,
};

static struct i2c_board_info touch_boardinfo[] __initdata = {	
	{ I2C_BOARD_INFO("kttech_o3_tsp", 0x4a),
		.platform_data = &qt602240_platform_data, 
		.irq  = MSM_GPIO_TO_INT(66),
	},
};
#endif

/* For MXT224/224E ver.3 drivers */
#if defined(CONFIG_KTTECH_TOUCH_QT602240V3) || defined(CONFIG_KTTECH_TOUCH_QT602240V3E)
struct class *kttech_class;
static struct mxt_callbacks *inform_charger_callbacks;
EXPORT_SYMBOL(kttech_class);

static void o3_touch_init_hw(struct mxt_platform_data *pdata)
{
	int rc;

	pr_info("(%s,%d)\n", __func__, __LINE__);

	/* I2C Port Config */
	gpio_tlmm_config(GPIO_CFG(36, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(35, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	
	if (gpio_request(pdata->reset_gpio, "ts_reset") < 0)
		rc = -1;

	if (gpio_request(pdata->irq_gpio, "ts_int") < 0)
		rc = -1;

	gpio_tlmm_config(GPIO_CFG(pdata->reset_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	rc = gpio_direction_output(pdata->reset_gpio, 1);

	gpio_tlmm_config(GPIO_CFG(pdata->irq_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	rc = gpio_direction_input(pdata->irq_gpio);
	msleep(10);
	

	if (rc) { 
		pr_info("[TSP] Initialized H/W Pin configuration Failed!");
	}
	else {
		pr_info("[TSP] Initialized H/W Pin configuration!!");
	}	

}

static void o3_touch_suspend_hw(struct mxt_platform_data *pdata)
{
	pr_info("(%s,%d)\n", __func__, __LINE__);
	/* first power off (off sequence: avdd -> vdd) */
	regulator_disable(pdata->reg_lvs3);
	regulator_disable(pdata->reg_l4);
	regulator_disable(pdata->reg_mvs0);

	/* GPIO pin configuration for MSM8x60 */
	gpio_tlmm_config(GPIO_CFG(36, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(35, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pdata->reset_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pdata->irq_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), GPIO_CFG_ENABLE);

	pr_info("[TSP] Power Off!!");
}

static void o3_touch_resume_hw(struct mxt_platform_data *pdata)
{
	pr_info("(%s,%d)\n", __func__, __LINE__);

	/* first pin configuration */
	/* GPIO pin configuration for MSM8x60 */
	gpio_tlmm_config(GPIO_CFG(36, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(35, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pdata->reset_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pdata->irq_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);

	/* and then, power on (on sequence: vdd -> avdd) */
	regulator_enable(pdata->reg_lvs3);
	regulator_enable(pdata->reg_l4);
	regulator_enable(pdata->reg_mvs0);
	msleep(100);
	pr_info("[TSP] Power On!!");
}

void o3_inform_charger_connection(int mode)
{
	if (inform_charger_callbacks &&
		inform_charger_callbacks->inform_charger)
	inform_charger_callbacks->inform_charger(inform_charger_callbacks, mode);
};
EXPORT_SYMBOL(o3_inform_charger_connection);

static void o3_register_touch_callbacks(struct mxt_callbacks *cb)
{
	inform_charger_callbacks = cb;
}

static struct mxt_platform_data qt602240_v3_platform_data = {
	.platform_name = "mxt224E TSP",
	.numtouch = 10,
	.max_x = 540,
	.max_y = 960, /* (960 = LCD ,  64 = touch) */
	.init_platform_hw = o3_touch_init_hw,
	.exit_platform_hw = NULL,
	.suspend_platform_hw = o3_touch_suspend_hw,
	.resume_platform_hw = o3_touch_resume_hw,
	.register_cb = o3_register_touch_callbacks,
	
	.reg_lvs3_name = "8901_lvs3",
	.reg_lvs3 = NULL,
	.reg_lvs3_level = 1800000,
	
	.reg_l4_name = "8901_l4",
	.reg_l4 = NULL,
	.reg_l4_level = 2700000,
	
	.reg_mvs0_name = "8901_mvs0",
	.reg_mvs0 = NULL,
	.reg_mvs0_level = 2700000,
	
	.irq_gpio = 66,
	.reset_gpio = 33,
	.board_rev = 1,
};

static struct i2c_board_info touch_boardinfo[] __initdata = {	
	{ I2C_BOARD_INFO("kttech_o3_tsp", 0x4a),    
		.platform_data = &qt602240_v3_platform_data, 
		.irq  = MSM_GPIO_TO_INT(66),
	},
};
#endif

/* For MELFAS MCS8000 series */
#ifdef CONFIG_KTTECH_TOUCH_MCS8000
struct class *kttech_class;
static struct melfas_callbacks *inform_charger_callbacks;

EXPORT_SYMBOL(kttech_class);

static void o3_touch_init_hw(struct melfas_ts_platform_data *pdata)
{
	int rc = 0;
	int error = 0;

	/* I2C Port Config */
	gpio_tlmm_config(GPIO_CFG(36, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(35, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);

	/* Set TSP IRQ */
	gpio_tlmm_config(GPIO_CFG(pdata->irq_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);

	/* Set Input Key IRQ */
	gpio_tlmm_config(GPIO_CFG(pdata->key_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);

	/* Initialize Platform Regulator Configuration for O3 */
	if (pdata->reg_lvs3 == NULL) {
		pdata->reg_lvs3 = regulator_get(NULL, pdata->reg_lvs3_name);

		if (IS_ERR(pdata->reg_lvs3)) { 
			error = PTR_ERR(pdata->reg_lvs3);
			pr_err("[TSP] [%s: %s]unable to get regulator %s: %d\n",
				__func__,
				pdata->platform_name,
				pdata->reg_lvs3_name,
				error);
		}
	}

	if (pdata->reg_l4 == NULL) {
		pdata->reg_l4 = regulator_get(NULL, pdata->reg_l4_name);

		if (IS_ERR(pdata->reg_l4)) { 
			error = PTR_ERR(pdata->reg_l4);
			pr_err("[TSP] [%s: %s]unable to get regulator %s: %d\n",
				__func__,
				pdata->platform_name,
				pdata->reg_l4_name,
				error);
		} 
		else {
			/* L4 Regulator Set Voltage */
			error = regulator_set_voltage(pdata->reg_l4,
				pdata->reg_l4_level,
				pdata->reg_l4_level);
		
			if (error) { 
				pr_err("[TSP] [%s: %s]Regulator Set Voltage (L4) failed! %s:(%d)\n",
					__func__,
					pdata->platform_name,
					pdata->reg_l4_name,
					error);
			}
		}
	}

	if (pdata->reg_mvs0 == NULL) {
		pdata->reg_mvs0 = regulator_get(NULL, pdata->reg_mvs0_name);

		if (IS_ERR(pdata->reg_mvs0)) { 
			error = PTR_ERR(pdata->reg_mvs0);
			pr_err("[TSP] [%s: %s]unable to get regulator %s: %d\n",
				__func__,
				pdata->platform_name,
				pdata->reg_mvs0_name,
				error);
		}
	}

	/* Enable Regulator */
	/* L4 */
	error += regulator_enable(pdata->reg_l4);
	/* LVS3 */
	error += regulator_enable(pdata->reg_lvs3);
	/* MVS0 */
	error += regulator_enable(pdata->reg_mvs0);
	
	msleep(20);

	if (error < 0){ 
		pr_err("[TSP] Could not set regulator. %s:(%d)\n", __func__, error);
	}

	if (rc < 0) { 
		pr_info("[TSP] TSP Hardware initialization has failed!");
	}
	else {
		pr_info("[TSP] TSP Hardware initialization was successful.");
	}

	return; 
}

static void o3_touch_suspend_hw(struct melfas_ts_platform_data *pdata)
{
	int rc = 0;
	
	/* First power off (off sequence: avdd -> vdd) */
	if(regulator_is_enabled(pdata->reg_mvs0) > 0) {
		rc = regulator_disable(pdata->reg_mvs0);
		if (rc < 0) { 
			pr_err("[TSP] regulator_disable failed!(reg_mvs0)");
		}
	}
	else {
		pr_err("[TSP] regulator already disabled. (reg_mvs0)");
	}
	
	if(regulator_is_enabled(pdata->reg_l4) > 0) {
		rc = regulator_disable(pdata->reg_l4);
		if (rc < 0) { 
			pr_err("[TSP] regulator_disable failed!(reg_l4)");
		}
	}
	else {
		pr_err("[TSP] regulator already disabled. (reg_l4)");
	}

	if(regulator_is_enabled(pdata->reg_lvs3) > 0) {
		rc = regulator_disable(pdata->reg_lvs3);
		if (rc < 0) { 
			pr_err("[TSP] regulator_disable failed!(reg_lvs3)");
		}
	}
	else {
		pr_err("[TSP] regulator already disabled. (reg_lvs3)");
	}

	/* GPIO pin configuration for MSM8x60 */
	gpio_tlmm_config(GPIO_CFG(36, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(35, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pdata->irq_gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pdata->key_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	msleep(5);

	return;
	
}

static void o3_touch_resume_hw(struct melfas_ts_platform_data *pdata)
{
	int rc = 0;
	
	/* First pin configuration */
	/* GPIO pin configuration for MSM8x60 */
	gpio_tlmm_config(GPIO_CFG(36, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(35, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pdata->irq_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(pdata->key_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	
	msleep(5);
	
	/* and then, power on (on sequence: vdd -> avdd) */
	rc = regulator_enable(pdata->reg_lvs3);
	if (rc < 0) { 
		pr_err("[TSP] regulator_enable failed!(reg_mvs0)");
	}
	rc = regulator_enable(pdata->reg_l4);
	if (rc < 0) { 
		pr_err("[TSP] regulator_enable failed!(reg_l4)");
	}
	rc = regulator_enable(pdata->reg_mvs0);
	if (rc < 0) { 
		pr_err("[TSP] regulator_enable failed!(reg_lvs3)");
	}
	msleep(20);

	return;
}

void o3_inform_charger_connection(int mode)
{
	if (inform_charger_callbacks && inform_charger_callbacks->inform_charger)
		inform_charger_callbacks->inform_charger(inform_charger_callbacks, mode);
};
EXPORT_SYMBOL(o3_inform_charger_connection);

static void o3_register_touch_callbacks(struct melfas_callbacks *cb)
{
	inform_charger_callbacks = cb;
}

static struct melfas_ts_platform_data mcs8000_platform_data = {
	.platform_name = "kttech_o3_tsp",
	.numtouch = 6,
	.max_x = 540,
	.max_y = 960, /* (960 = LCD ,  64 = touch) */
	.init_platform_hw = o3_touch_init_hw,
	.exit_platform_hw = NULL,
	.suspend_platform_hw = o3_touch_suspend_hw,
	.resume_platform_hw = o3_touch_resume_hw,
	.register_cb = o3_register_touch_callbacks,
	
	.reg_lvs3_name = "8901_lvs3",
	.reg_lvs3 = NULL,
	.reg_lvs3_level = 1800000,
	
	.reg_l4_name = "8901_l4",
	.reg_l4 = NULL,
	.reg_l4_level = 2800000,
	
	.reg_mvs0_name = "8901_mvs0",
	.reg_mvs0 = NULL,
	.reg_mvs0_level = 2800000,
	
	.irq_gpio = 66,
	.key_gpio = 172, 
};

static struct i2c_board_info touch_boardinfo[] __initdata = {	
	{ I2C_BOARD_INFO("kttech_o3_tsp", 0x48),    
		.platform_data = &mcs8000_platform_data, 
		.irq  = MSM_GPIO_TO_INT(66),
	},
};
#endif

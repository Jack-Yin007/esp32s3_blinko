#include <string.h>
#include "driver/i2c.h"
#include "drivers/charger.h"
#include "esp_log.h"

static const char *TAG = "CHARGER";

#define CW6305_I2C_PORT     I2C_NUM_0
#define CW6305_I2C_ADDR     0x0B
#define CW6305_I2C_TIMEOUT  pdMS_TO_TICKS(1000)

static esp_err_t
cw6305_read_reg(uint8_t reg_addr, uint8_t *reg_data)
{
	if (reg_data == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (CW6305_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg_addr, true);

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (CW6305_I2C_ADDR << 1) | I2C_MASTER_READ, true);
	i2c_master_read_byte(cmd, reg_data, I2C_MASTER_NACK);
	i2c_master_stop(cmd);

	esp_err_t err = i2c_master_cmd_begin(CW6305_I2C_PORT, cmd, CW6305_I2C_TIMEOUT);
	i2c_cmd_link_delete(cmd);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Read CW6305 register %x failed: %s", reg_addr, esp_err_to_name(err));
	}

	return err;
}

static esp_err_t
cw6305_write_reg(uint8_t reg_addr, uint8_t reg_data)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (CW6305_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg_addr, true);
	i2c_master_write_byte(cmd, reg_data, true);
	i2c_master_stop(cmd);

	esp_err_t err = i2c_master_cmd_begin(CW6305_I2C_PORT, cmd, CW6305_I2C_TIMEOUT);
	i2c_cmd_link_delete(cmd);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Write CW6305 register %x failed: %s", reg_addr, esp_err_to_name(err));
	}

	return err;
}

static inline esp_err_t
cw6305_disable_watchdog(void)
{
	const uint8_t reg_addr = CW6305_REG_SAFETY_CFG;
	union cw6305_reg_safety_cfg reg_data;

	esp_err_t err = cw6305_read_reg(reg_addr, &reg_data.b);
	if (err != ESP_OK) return err;

	reg_data.wd_chg = 0;
	err = cw6305_write_reg(reg_addr, reg_data.b);
	return err;
}

static inline esp_err_t
cw6305_set_charge_voltage(int voltage)
{
	const uint8_t reg_addr = CW6305_REG_CHG_VOLT;
	union cw6305_reg_chg_volt reg_data;

	esp_err_t err = cw6305_read_reg(reg_addr, &reg_data.b);
	if (err != ESP_OK) return err;

	reg_data.vbreg = voltage;
	err = cw6305_write_reg(reg_addr, reg_data.b);
	return err;
}

static inline esp_err_t
cw6305_set_charge_current(int current)
{
	const uint8_t reg_addr = CW6305_REG_CHG_CUR1;
	union cw6305_reg_chg_cur1 reg_data;

	esp_err_t err = cw6305_read_reg(reg_addr, &reg_data.b);
	if (err != ESP_OK) return err;

	reg_data.ichg = current;
	err = cw6305_write_reg(reg_addr, reg_data.b);
	return err;
}

static inline esp_err_t
cw6305_set_oc_protection(int current)
{
	const uint8_t reg_addr = CW6305_REG_SAFETY_CFG2;
	union cw6305_reg_safety_cfg2 reg_data;

	esp_err_t err = cw6305_read_reg(reg_addr, &reg_data.b);
	if (err != ESP_OK) return err;

	reg_data.sys_ocp = current;
	err = cw6305_write_reg(reg_addr, reg_data.b);
	return err;
}

static inline esp_err_t
cw6305_enable(bool enable)
{
	const uint8_t reg_addr = CW6305_REG_VBUS_VOLT;
	union cw6305_reg_vbus_volt vbus_volt;

	esp_err_t err = cw6305_read_reg(reg_addr, &vbus_volt.b);
	if (err != ESP_OK) return err;

	vbus_volt.chg_en = enable;
	err = cw6305_write_reg(reg_addr, vbus_volt.b);
	return err;
}

static inline esp_err_t
cw6305_shipping_mode(bool enable)
{
        const uint8_t reg_addr = CW6305_REG_CONFIG;
        union cw6305_reg_config config;

        esp_err_t err = cw6305_read_reg(reg_addr, &config.b);
        if (err != ESP_OK) return err;

        config.batfet_off = enable;
        err = cw6305_write_reg(reg_addr, config.b);
	return err;
}

esp_err_t
charger_shutdown(bool shutdown)
{
	return cw6305_shipping_mode(shutdown);
}

esp_err_t
charger_init(struct charger_config_t *config)
{
	esp_err_t err = ESP_OK;

	uint8_t reg_data;
	err = cw6305_read_reg(CW6305_REG_VERSIONS, &reg_data);
	if (err != ESP_OK) return err;

	ESP_LOGI(TAG, "VERSIONS = %xh", reg_data);
	if (reg_data != CW6305_VERSIONS_DEFAULT)
		return ESP_ERR_INVALID_VERSION;

	err = cw6305_disable_watchdog();
	if (err != ESP_OK) return err;

	err = cw6305_set_charge_voltage(config->charge_voltage);
	if (err != ESP_OK) return err;

	err = cw6305_set_charge_current(config->charge_current);
	if (err != ESP_OK) return err;

	err = cw6305_set_oc_protection(config->over_current_protection);
	if (err != ESP_OK) return err;

	err = cw6305_enable(true);
	return err;
}


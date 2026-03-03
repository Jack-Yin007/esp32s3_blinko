#include <string.h>
#include "driver/i2c.h"
#include "drivers/charger.h"
#include "esp_log.h"

static const char *TAG = "CHARGER";

#define CW6116_I2C_ADDR     0x6B
#define CW6116_I2C_PORT     I2C_NUM_0
#define CW6116_I2C_TIMEOUT  pdMS_TO_TICKS(1000)

static esp_err_t
cw6116_read_reg(uint8_t reg_addr, uint8_t *reg_data)
{
	if (reg_data == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (CW6116_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg_addr, true);

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (CW6116_I2C_ADDR << 1) | I2C_MASTER_READ, true);
	i2c_master_read_byte(cmd, reg_data, I2C_MASTER_NACK);
	i2c_master_stop(cmd);

	esp_err_t err = i2c_master_cmd_begin(CW6116_I2C_PORT, cmd, CW6116_I2C_TIMEOUT);
	i2c_cmd_link_delete(cmd);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Read register %x failed: %s", reg_addr, esp_err_to_name(err));
	}

	return err;
}

static esp_err_t
cw6116_write_reg(uint8_t reg_addr, uint8_t reg_data)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (CW6116_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg_addr, true);
	i2c_master_write_byte(cmd, reg_data, true);
	i2c_master_stop(cmd);

	esp_err_t err = i2c_master_cmd_begin(CW6116_I2C_PORT, cmd, CW6116_I2C_TIMEOUT);
	i2c_cmd_link_delete(cmd);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Write register %x failed: %s", reg_addr, esp_err_to_name(err));
	}

	return err;
}

static inline esp_err_t
cw6116_disable_watchdog(void)
{
	const uint8_t reg_addr = 0x05;
	union {
		uint8_t b;
		struct {
			uint8_t jeita_iset : 1;
			uint8_t treg       : 1;
			uint8_t chg_timer  : 1;
			uint8_t en_timer   : 1;
			uint8_t watchdog   : 2;
			uint8_t hvdcp_en   : 1;
			uint8_t en_term    : 1;
		};
	} reg_data;

	esp_err_t err = cw6116_read_reg(reg_addr, &reg_data.b);
	if (err != ESP_OK) return err;

	reg_data.watchdog = 0;
	err = cw6116_write_reg(reg_addr, reg_data.b);
	return err;
}

static inline esp_err_t
cw6116_set_charge_voltage(int voltage)
{
	const uint8_t reg_addr = 0x04;
	union {
		uint8_t b;
		struct {
			uint8_t vrechg       : 1;
			uint8_t topoff_timer : 2;
			uint8_t vreg         : 5;
		};
	} reg_data;

	esp_err_t err = cw6116_read_reg(reg_addr, &reg_data.b);
	if (err != ESP_OK) return err;

	reg_data.vreg = voltage;
	err = cw6116_write_reg(reg_addr, reg_data.b);
	return err;
}

static inline esp_err_t
cw6116_set_charge_current(int current)
{
	const uint8_t reg_addr = 0x02;
	union {
		uint8_t b;
		struct {
			uint8_t ichg      : 6;
			uint8_t rsvd_6    : 1;
			uint8_t boost_lim : 1;
		};
	} reg_data;

	esp_err_t err = cw6116_read_reg(reg_addr, &reg_data.b);
	if (err != ESP_OK) return err;

	reg_data.ichg = current;
	err = cw6116_write_reg(reg_addr, reg_data.b);
	return err;
}

static inline esp_err_t
cw6116_set_oc_protection(int current)
{
	const uint8_t reg_addr = 0x00;
	union {
		uint8_t b;
		struct {
			uint8_t iindpm      : 5;
			uint8_t en_ichg_mon : 2;
			uint8_t en_hiz      : 1;
		};
	} reg_data;

	esp_err_t err = cw6116_read_reg(reg_addr, &reg_data.b);
	if (err != ESP_OK) return err;

	reg_data.iindpm = current;
	err = cw6116_write_reg(reg_addr, reg_data.b);
	return err;
}

static inline esp_err_t
cw6116_enable(bool enable)
{
	const uint8_t reg_addr = 0x01;
	union {
		uint8_t b;
		struct {
			uint8_t min_vbat_sel : 1;
			uint8_t sys_min      : 3;
			uint8_t chg_config   : 1;
			uint8_t otg_config   : 1;
			uint8_t wd_rst       : 1;
			uint8_t rsvd_7       : 1;
		};
	} reg_data;

	esp_err_t err = cw6116_read_reg(reg_addr, &reg_data.b);
	if (err != ESP_OK) return err;

	reg_data.wd_rst = 1;
	reg_data.chg_config = enable;
	err = cw6116_write_reg(reg_addr, reg_data.b);
	return err;
}

static inline esp_err_t
cw6116_shipping_mode(bool enable)
{
        const uint8_t reg_addr = 0x07;
        union {
		uint8_t b;
		struct {
			uint8_t vdpm_bat_track : 2;
			uint8_t batfet_rst_en  : 1;
			uint8_t batfet_dly     : 1;
			uint8_t jeita_vset     : 1;
			uint8_t batfet_dis     : 1;
			uint8_t tmr2x_en       : 1;
			uint8_t iindet_en      : 1;
		};
	} reg_data;

        esp_err_t err = cw6116_read_reg(reg_addr, &reg_data.b);
        if (err != ESP_OK) return err;

        reg_data.batfet_dis = enable;
        err = cw6116_write_reg(reg_addr, reg_data.b);
	return err;
}

esp_err_t
charger_shutdown(bool shutdown)
{
	return cw6116_shipping_mode(shutdown);
}

esp_err_t
charger_init(struct charger_config_t *config)
{
	esp_err_t err = ESP_OK;

	uint8_t reg_data;
	err = cw6116_read_reg(0x0B, &reg_data);
	if (err != ESP_OK) return err;

	ESP_LOGI(TAG, "VERSIONS = %xh", reg_data);
	if (reg_data != 0x3B)
		return ESP_ERR_INVALID_VERSION;

	err = cw6116_enable(true);
	if (err != ESP_OK) return err;

	err = cw6116_disable_watchdog();
	if (err != ESP_OK) return err;

	err = cw6116_set_charge_voltage(config->charge_voltage);
	if (err != ESP_OK) return err;

	err = cw6116_set_charge_current(config->charge_current);
	if (err != ESP_OK) return err;

	err = cw6116_set_oc_protection(config->over_current_protection);

	for (int reg_addr = 0x00; reg_addr < 0x0C; ++reg_addr)
	{
		if (cw6116_read_reg(reg_addr, &reg_data) != 0) break;
		ESP_LOGI(TAG, "[%02X] = %02x", reg_addr, reg_data);
	}

	return err;
}


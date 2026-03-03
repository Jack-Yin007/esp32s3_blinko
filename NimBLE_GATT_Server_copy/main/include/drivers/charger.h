#ifndef _CHARGER_H
#define _CHARGER_H

#define CW6305_VERSIONS_DEFAULT 0x14

enum cw6305_reg
{
	CW6305_REG_VERSIONS     = 0x00,
	CW6305_REG_VBUS_VOLT    = 0x01,
	CW6305_REG_VBUS_CUR     = 0x02,
	CW6305_REG_CHG_VOLT     = 0x03,
	CW6305_REG_CHG_CUR1     = 0x04,
	CW6305_REG_CHG_CUR2     = 0x05,
	CW6305_REG_CHG_SAFETY   = 0x06,
	CW6305_REG_CONFIG       = 0x07,
	CW6305_REG_SAFETY_CFG   = 0x08,
	CW6305_REG_SAFETY_CFG2  = 0x09,
	CW6305_REG_INT_SET      = 0x0A,
	CW6305_REG_INT_SRC      = 0x0B,
	CW6305_REG_IC_STATUS    = 0x0D,
	CW6305_REG_IC_STATUS2   = 0x0E,
};

union cw6305_reg_vbus_volt
{
	uint8_t b;
	struct
	{
		uint8_t vbus_dpm    : 4;  // 03:00 VBUS_DPM[3:0]
		uint8_t _rsvd_4     : 2;  // 05:04 reserved
		uint8_t chg_en      : 1;  // 06 CHG_EN
		uint8_t en_hiz      : 1;  // 07 EN_HIZ
	};
};

union cw6305_reg_chg_volt
{
	uint8_t b;
	struct
	{
		uint8_t batlowv     : 1;  // 00 BATLOWV
		uint8_t vrechg      : 1;  // 01 VRECHG
		uint8_t vbreg       : 6;  // 07:02 VBREG[5:0]
	};
};

union cw6305_reg_chg_cur1
{
	uint8_t b;
	struct
	{
		uint8_t ichg        : 6;  // 05:00 ICHG[5:0]
		uint8_t treg        : 2;  // 07:06 TREG[1:0]
	};
};

union cw6305_reg_safety_cfg
{
	uint8_t b;
	struct
	{
		uint8_t bat_uvlo    : 3;  // 02:00 BAT_UVLO[2:0]
		uint8_t _rsvd_3     : 1;  // 03 reserved
		uint8_t watch       : 2;  // 05:04 WATCH[1:0]
		uint8_t wd_dischg   : 1;  // 06 WD_DISCHG
		uint8_t wd_chg      : 1;  // 07 WD_CHG
	};
};

union cw6305_reg_safety_cfg2
{
	uint8_t b;
	struct
	{
		uint8_t sys_ocp     : 4;  // 03:00 SYS_OCP
		uint8_t _rsvd_4     : 3;  // 06:04 reserved
		uint8_t tmr2x_en    : 1;  // 07 TMR2X_EN
	};
};

union cw6305_reg_config
{
	uint8_t b;
	struct
	{
		uint8_t int_off_timer   : 2;  // 01:00 INT_OFF_TIMER[1:0]
		uint8_t off_last_time   : 1;  // 02 OFF_LAST_TIME
		uint8_t button_func     : 1;  // 03 BUTTON_FUNC
		uint8_t en_int_button   : 1;  // 04 EN_INT_BUTTON
		uint8_t off_delay       : 2;  // 06:05 OFF_DELAY[1:0]
		uint8_t batfet_off      : 1;  // 07 BATFET_OFF
	};
};

struct charger_config_t
{
	enum
	{
		CW6305_CHARGE_VOLTAGE_4V20 = 0x1C,
		CW6305_CHARGE_VOLTAGE_4V35 = 0x28,

		CW6116_CHARGE_VOLTAGE_4V20 = 0x0E,
		CW6116_CHARGE_VOLTAGE_4V35 = 0x14,
	} charge_voltage;

	enum
	{
		CW6305_CHARGE_CURRENT_20mA_00 = 0x08,
		CW6305_CHARGE_CURRENT_470mA_00 = 0x3F,

		CW6116_CHARGE_CURRENT_2000mA = 0x28,
	} charge_current;

	enum
	{
		CW6305_OC_PROTECTION_1000mA = 0x09,
		CW6305_OC_PROTECTION_1600mA = 0x0F,

		CW6116_OC_PROTECTION_2400mA = 0x17,
	} over_current_protection;
};

esp_err_t
charger_shutdown(bool shutdown);

esp_err_t
charger_init(struct charger_config_t *config);

#endif	/* _CHARGER_H */


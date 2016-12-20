/*
 * portabook_battery.c - Portabook battery driver
 * Copyright (C) 2016  MURAMATSU Atsushi <amura@tomato.sakura.ne.jp>
 *
 * This code is based on
 *   driver/acpi/battery.c and driver/power/ds2760_battery.c
 *   from linux-4.4 kernel
 */
/*
 *  battery.c - ACPI Battery Driver (Revision: 2.0)
 *
 *  Copyright (C) 2007 Alexey Starikovskiy <astarikovskiy@suse.de>
 *  Copyright (C) 2004-2007 Vladimir Lebedev <vladimir.p.lebedev@intel.com>
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
/*
 * Driver for batteries with DS2760 chips inside.
 *
 * Copyright © 2007 Anton Vorontsov
 *	       2004-2007 Matt Reimer
 *	       2004 Szabolcs Gyurko
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * Author:  Anton Vorontsov <cbou@mail.ru>
 *	    February 2007
 *
 *	    Matt Reimer <mreimer@vpop.net>
 *	    April 2004, 2005, 2007
 *
 *	    Szabolcs Gyurko <szabolcs.gyurko@tlt.hu>
 *	    September 2004
 */

#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>

#ifndef ACPI_BATTERY_STATE_CHARGING
#define ACPI_BATTERY_STATE_CHARGING	0x00000001
#endif
#ifndef ACPI_BATTERY_STATE_DISCHARGING
#define ACPI_BATTERY_STATE_DISCHARGING	0x00000002
#endif
#ifndef ACPI_BATTERY_STATE_CRITICAL
#define ACPI_BATTERY_STATE_CRITICAL	0x00000004
#endif
#ifndef ACPI_BATTERY_VALUE_UNKNOWN
#define ACPI_BATTERY_VALUE_UNKNOWN	0xFFFFFFFF
#endif

#define DESIGN_CAPACITY			4800	/* 4800 mAh */
#define DESIGN_VOLTAGE			3800	/* 3.8V */
#define DESIGN_WARN_CAPACITY		800	/* 800 mAh */
#define SMALL_DISCHARGE_RATE		200	/* 200mA is minumum? */

#define BATT_INDEX_REG			0x82
#define BATT_DATA_REG			0x80

#define	BATT_INFO_LAST_CAP_H		0x144
#define	BATT_INFO_LAST_CAP_L		0x145
#define	BATT_INFO_STATUS_H		0x1A0
#define	BATT_INFO_STATUS_L		0x1A1
#define	BATT_INFO_PRESENT_RATE_H	0x1A2
#define	BATT_INFO_PRESENT_RATE_L	0x1A3
#define	BATT_INFO_REMAIN_CAP_H		0x1A4
#define	BATT_INFO_REMAIN_CAP_L		0x1A5
#define	BATT_INFO_PRESENT_VOLT_H	0x1A6
#define	BATT_INFO_PRESENT_VOLT_L	0x1A7

struct portabook_battery {
    struct device *dev;
    struct power_supply *bat;
    struct power_supply_desc bat_desc;
    struct i2c_client *i2c_dev;
    
    /* portabook battery data,
       valid after calling portabook_batt_battery_read_status() */
    unsigned long update_time;	/* jiffies when data read */
    
    int rate_now;
    int capacity_now;
    int voltage_now;
    int full_charge_capacity;
    int state;
    
    struct workqueue_struct *monitor_wqueue;
    struct delayed_work monitor_work;
};

struct portabook_ac {
    struct power_supply *ac;
    struct power_supply_desc ac_desc;
    struct portabook_battery *battery;
};

static unsigned int cache_time = 1000;
module_param(cache_time, uint, 0644);
MODULE_PARM_DESC(cache_time, "cache time in milliseconds");

static int portabook_ac_init(struct portabook_battery *battery);

static int
read_battinfo_reg(struct i2c_client *i2c_dev, int reg, u8 *value)
{
    int s;
    char buf[3];

    /* set info reg */
    buf[0] = BATT_INDEX_REG;
    buf[1] = reg >> 8;
    buf[2] = reg & 0xff;
    s = i2c_master_send(i2c_dev, buf, 3);
    if (s < 0) return s;
    
    /* set to read reg */
    buf[0] = BATT_DATA_REG;
    s = i2c_master_send(i2c_dev, buf, 1);
    if (s < 0) return s;
    
    /* read data */
    s = i2c_master_recv(i2c_dev, value, 1);
    if (s < 0) return s;
    return 0;
}

static int
portabook_battery_read_status(struct portabook_battery *di)
{
    u8 buf[2];
    int s;
    
    if (di->update_time && time_before(jiffies, di->update_time +
				       msecs_to_jiffies(cache_time)))
	return 0;

    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_LAST_CAP_H, &buf[0]);
    if (s < 0) goto error;
    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_LAST_CAP_L, &buf[1]);
    if (s < 0) goto error;
    di->full_charge_capacity = buf[0] << 8 | buf[1];

    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_STATUS_H, &buf[0]);
    if (s < 0) goto error;
    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_STATUS_L, &buf[1]);
    if (s < 0) goto error;
    di->state = buf[0] << 8 | buf[1];

    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_PRESENT_RATE_H, &buf[0]);
    if (s < 0) goto error;
    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_PRESENT_RATE_L, &buf[1]);
    if (s < 0) goto error;
    di->rate_now = buf[0] << 8 | buf[1];

    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_REMAIN_CAP_H, &buf[0]);
    if (s < 0) goto error;
    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_REMAIN_CAP_L, &buf[1]);
    if (s < 0) goto error;
    di->capacity_now = buf[0] << 8 | buf[1];

    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_PRESENT_VOLT_H, &buf[0]);
    if (s < 0) goto error;
    s = read_battinfo_reg(di->i2c_dev, BATT_INFO_PRESENT_VOLT_L, &buf[1]);
    if (s < 0) goto error;
    di->voltage_now = buf[0] << 8 | buf[1];

    di->update_time = jiffies;
    return 0;
	
 error:
    dev_warn(di->dev, "call to read_battinfo_reg failed\n");
    return 1;
}

static void
portabook_battery_work(struct work_struct *work)
{
    struct portabook_battery *di =
	container_of(work, struct portabook_battery,
		     monitor_work.work);
    const int interval = HZ * 60;

    dev_dbg(di->dev, "%s\n", __func__);

    portabook_battery_read_status(di);
    queue_delayed_work(di->monitor_wqueue, &di->monitor_work, interval);
}

static int
portabook_battery_is_charged(struct portabook_battery *battery)
{
    /* charging or discharing with high rate */
    if (battery->state & ACPI_BATTERY_STATE_CHARGING)
	return 0;
    if (battery->state & ACPI_BATTERY_STATE_DISCHARGING &&
	battery->rate_now >= SMALL_DISCHARGE_RATE)
	return 0;

    /* battery not reporting charge */
    if (battery->capacity_now == ACPI_BATTERY_VALUE_UNKNOWN ||
	battery->capacity_now == 0)
	return 0;
    
    /* good batteries update full_charge as the batteries degrade */
    if (battery->full_charge_capacity*95 <= battery->capacity_now*100)
	return 1;
    
    /* fallback to using design values for broken batteries */
    if (DESIGN_CAPACITY >= battery->capacity_now)
	return 1;
    
    /* we don't do any sort of metric based on percentages */
    return 0;
}

static int
portabook_battery_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
    int ret = 0;
    struct portabook_battery *battery = power_supply_get_drvdata(psy);
    
    portabook_battery_read_status(battery);
    
    switch (psp) {
    case POWER_SUPPLY_PROP_STATUS:
	if (battery->state & ACPI_BATTERY_STATE_DISCHARGING)
	    val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
	else if (battery->state & ACPI_BATTERY_STATE_CHARGING)
	    val->intval = POWER_SUPPLY_STATUS_CHARGING;
	else if (portabook_battery_is_charged(battery))
	    val->intval = POWER_SUPPLY_STATUS_FULL;
	else
	    val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
	break;
	
    case POWER_SUPPLY_PROP_PRESENT:
	val->intval = 1;
	break;
	
    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
	val->intval = battery->voltage_now * 1000;
	break;
	
    case POWER_SUPPLY_PROP_CURRENT_NOW:
    case POWER_SUPPLY_PROP_POWER_NOW:
	val->intval = battery->rate_now * 1000;
	break;
	
    case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
    case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
	val->intval = DESIGN_CAPACITY * 1000;
	break;
	
    case POWER_SUPPLY_PROP_CHARGE_FULL:
    case POWER_SUPPLY_PROP_ENERGY_FULL:
	val->intval = battery->full_charge_capacity * 1000;
	break;
	
    case POWER_SUPPLY_PROP_CHARGE_NOW:
    case POWER_SUPPLY_PROP_ENERGY_NOW:
	val->intval = battery->capacity_now * 1000;
	break;
	
    case POWER_SUPPLY_PROP_CAPACITY:
	if (battery->capacity_now && battery->full_charge_capacity)
	    val->intval = battery->capacity_now * 100 /
		battery->full_charge_capacity;
	else
	    val->intval = 0;
	break;
	
    case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
	if (battery->state & ACPI_BATTERY_STATE_CRITICAL)
	    val->intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else if (battery->capacity_now <= DESIGN_WARN_CAPACITY)
	    val->intval = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (portabook_battery_is_charged(battery))
	    val->intval = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else
	    val->intval = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	break;
	
    default:
	ret = -EINVAL;
    }
    return ret;
}

static enum power_supply_property portabook_battery_props[] = {
    POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_CURRENT_NOW,
    POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
    POWER_SUPPLY_PROP_CHARGE_FULL,
    POWER_SUPPLY_PROP_CHARGE_EMPTY,
    POWER_SUPPLY_PROP_CHARGE_NOW,
    POWER_SUPPLY_PROP_CAPACITY,
    POWER_SUPPLY_PROP_CAPACITY_LEVEL,
};

static int
portabook_battery_probe(struct platform_device *pdev)
{
    struct power_supply_config psy_cfg = {};
    int retval = 0;
    struct portabook_battery *di;
    
    di = devm_kzalloc(&pdev->dev, sizeof(*di), GFP_KERNEL);
    if (!di) {
	retval = -ENOMEM;
	goto di_alloc_failed;
    }
    
    platform_set_drvdata(pdev, di);
    
    di->dev			= &pdev->dev;
    di->i2c_dev			= pdev->dev.parent;
    di->bat_desc.name		= dev_name(&pdev->dev);
    di->bat_desc.type		= POWER_SUPPLY_TYPE_BATTERY;
    di->bat_desc.properties	= portabook_battery_props;
    di->bat_desc.num_properties	= ARRAY_SIZE(portabook_battery_props);
    di->bat_desc.get_property	= portabook_battery_get_property;
    
    psy_cfg.drv_data		= di;
    
    /* enable sleep mode feature */
    portabook_battery_read_status(di);
    
    di->bat = power_supply_register(&pdev->dev, &di->bat_desc, &psy_cfg);
    if (IS_ERR(di->bat)) {
	dev_err(di->dev, "failed to register battery\n");
	retval = PTR_ERR(di->bat);
	goto batt_failed;
    }

    INIT_DELAYED_WORK(&di->monitor_work, portabook_battery_work);
    di->monitor_wqueue = create_singlethread_workqueue(dev_name(&pdev->dev));
    if (!di->monitor_wqueue) {
	retval = -ESRCH;
	goto workqueue_failed;
    }
    queue_delayed_work(di->monitor_wqueue, &di->monitor_work, HZ * 1);

    retval = portabook_ac_init(di->bat);
    if (retval = 0)
	goto success;
    
workqueue_failed:
    power_supply_unregister(di->bat);
    
batt_failed:
di_alloc_failed:
success:
    return retval;
}

static int
portabook_battery_remove(struct platform_device *pdev)
{
    struct portabook_battery *di = platform_get_drvdata(pdev);
    
    cancel_delayed_work_sync(&di->monitor_work);
    destroy_workqueue(di->monitor_wqueue);
    power_supply_unregister(di->bat);
    
    return 0;
}

static int
portabook_ac_is_connected(struct portabook_battery *battery)
{
    if (battery->state & ACPI_BATTERY_STATE_CHARGING)
	return 1;
    if (battery->state & ACPI_BATTERY_STATE_DISCHARGING &&
	battery->rate_now >= SMALL_DISCHARGE_RATE)
	return 0;
    return 1;
}

static int
portabook_get_ac_property(struct power_supply *psy,
			  enum power_supply_property psp,
			  union power_supply_propval *val)
{
    struct portabook_ac *ac = power_supply_get_drvdata(psy);
    if (!ac)
	return -ENODEV;

    portabook_battery_read_status(ac->battery);
    
    switch (psp) {
    case POWER_SUPPLY_PROP_ONLINE:
	val->intval = portabook_ac_is_connected(ac->battery);
	break;
    default:
	return -EINVAL;
    }
    return 0;
}

static enum power_supply_property portabook_ac_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
};


static int portabook_ac_init(struct portabook_battery *battery)
{
    struct power_supply_config psy_cfg = {};
    int result = 0;
    struct portabook_ac *ac = NULL;

    ac = kzalloc(sizeof(struct portabook_ac), GFP_KERNEL);
    if (!ac)
	return -ENOMEM;
    
    ac->battery = battery;
    ac->ac_desc.name = battery->bat_desc.name;
    ac->ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
    ac->ac_desc.properties = portabook_ac_props;
    ac->ac_desc.num_properties = ARRAY_SIZE(portabook_ac_props);
    ac->ac_desc.get_property = portabook_get_ac_property;
    ac->ac = power_supply_register(&ac->battery->dev,
				   &ac->ac_desc, &psy_cfg);
    if (IS_ERR(ac->ac)) {
	result = PTR_ERR(ac->ac);
	goto end;
    }
end:
    if (result) {
	kfree(ac);
    }
    return result;
}


#ifdef CONFIG_PM

static int
portabook_battery_suspend(struct platform_device *pdev,
			       pm_message_t state)
{
    return 0;
}

static int
portabook_battery_resume(struct platform_device *pdev)
{
    struct portabook_battery *di = platform_get_drvdata(pdev);
    //power_supply_changed(di->bat);
    mod_delayed_work(di->monitor_wqueue, &di->monitor_work, HZ);
    return 0;
}

#else

#define portabook_battery_suspend NULL
#define portabook_battery_resume NULL

#endif /* CONFIG_PM */

static struct platform_driver portabook_battery_driver = {
    .driver = {
	.name = "portabook-battery",
    },
    .probe    = portabook_battery_probe,
    .remove   = portabook_battery_remove,
    .suspend  = portabook_battery_suspend,
    .resume   = portabook_battery_resume,
};

module_platform_driver(portabook_battery_driver);

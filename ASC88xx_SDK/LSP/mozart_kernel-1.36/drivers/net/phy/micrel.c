/*
 * drivers/net/phy/micrel.c
 *
 * Driver for Micrel PHYs
 *
 * Author: David J. Choi
 *
 * Copyright (c) 2010 Micrel, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * Support : ksz9021 1000/100/10 phy from Micrel
 *		ks8001, ks8737, ks8721, ks8041, ks8051 100/10 phy
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/micrel_phy.h>

/* general Interrupt control/status reg in vendor specific block. */
#define MII_KSZPHY_INTCS			0x1B
#define	KSZPHY_INTCS_JABBER			(1 << 15)
#define	KSZPHY_INTCS_RECEIVE_ERR		(1 << 14)
#define	KSZPHY_INTCS_PAGE_RECEIVE		(1 << 13)
#define	KSZPHY_INTCS_PARELLEL			(1 << 12)
#define	KSZPHY_INTCS_LINK_PARTNER_ACK		(1 << 11)
#define	KSZPHY_INTCS_LINK_DOWN			(1 << 10)
#define	KSZPHY_INTCS_REMOTE_FAULT		(1 << 9)
#define	KSZPHY_INTCS_LINK_UP			(1 << 8)
#define	KSZPHY_INTCS_ALL			(KSZPHY_INTCS_LINK_UP |\
						KSZPHY_INTCS_LINK_DOWN)

/* general PHY control reg in vendor specific block. */
#define	MII_KSZPHY_CTRL			0x1F
/* bitmap of PHY register to set interrupt mode */
#define KSZPHY_CTRL_INT_ACTIVE_HIGH		(1 << 9)
#define KSZ9021_CTRL_INT_ACTIVE_HIGH		(1 << 14)
#define KS8737_CTRL_INT_ACTIVE_HIGH		(1 << 14)
#define KSZ8051_RMII_50MHZ_CLK			(1 << 7)

static int kszphy_ack_interrupt(struct phy_device *phydev)
{
	/* bit[7..0] int status, which is a read and clear register. */
	int rc;

	rc = phy_read(phydev, MII_KSZPHY_INTCS);

	return (rc < 0) ? rc : 0;
}

static int kszphy_set_interrupt(struct phy_device *phydev)
{
	int temp;
	temp = (PHY_INTERRUPT_ENABLED == phydev->interrupts) ?
		KSZPHY_INTCS_ALL : 0;
	return phy_write(phydev, MII_KSZPHY_INTCS, temp);
}

static int kszphy_config_intr(struct phy_device *phydev)
{
	int temp, rc;

	/* set the interrupt pin active low */
	temp = phy_read(phydev, MII_KSZPHY_CTRL);
	temp &= ~KSZPHY_CTRL_INT_ACTIVE_HIGH;
	phy_write(phydev, MII_KSZPHY_CTRL, temp);
	rc = kszphy_set_interrupt(phydev);
	return rc < 0 ? rc : 0;
}

static int ksz9021_config_intr(struct phy_device *phydev)
{
	int temp, rc;

	/* set the interrupt pin active low */
	temp = phy_read(phydev, MII_KSZPHY_CTRL);
	temp &= ~KSZ9021_CTRL_INT_ACTIVE_HIGH;
	phy_write(phydev, MII_KSZPHY_CTRL, temp);
	rc = kszphy_set_interrupt(phydev);
	return rc < 0 ? rc : 0;
}

static int ks8737_config_intr(struct phy_device *phydev)
{
	int temp, rc;

	/* set the interrupt pin active low */
	temp = phy_read(phydev, MII_KSZPHY_CTRL);
	temp &= ~KS8737_CTRL_INT_ACTIVE_HIGH;
	phy_write(phydev, MII_KSZPHY_CTRL, temp);
	rc = kszphy_set_interrupt(phydev);
	return rc < 0 ? rc : 0;
}

static int kszphy_config_init(struct phy_device *phydev)
{
	return 0;
}

static int kszphy_reset(struct phy_device *phydev)
{
	int bmcr;

	/* Software Reset PHY */
	bmcr = phy_read(phydev, MII_BMCR);
	if (bmcr < 0)
		return bmcr;
	bmcr |= BMCR_RESET;
	bmcr = phy_write(phydev, MII_BMCR, bmcr);
	if (bmcr < 0)
		return bmcr;

	do {
		bmcr = phy_read(phydev, MII_BMCR);
		if (bmcr < 0)
			return bmcr;
	} while (bmcr & BMCR_RESET);

	return 0;
}

static int ksz8051_config_init(struct phy_device *phydev)
{
	int regval;

	kszphy_reset(phydev);

	if (phydev->dev_flags & MICREL_PHY_50MHZ_CLK) {
		regval = phy_read(phydev, MII_KSZPHY_CTRL);
		regval |= KSZ8051_RMII_50MHZ_CLK;
		phy_write(phydev, MII_KSZPHY_CTRL, regval);
	}

	return 0;
}

static int ksz8051_config_aneg(struct phy_device *phydev)
{
	int err;
	int bmcr;

	if (AUTONEG_ENABLE == phydev->autoneg) {
		bmcr = phy_read(phydev, MII_BMCR);
		if (bmcr < 0)
			return bmcr;
		/*
		 * KSZ8051 seems have a bug when auto-negotiation half duplex mode if BMCR_FULLDPLX is set.
		 * Apply half mode as default here
		 * (workaround and waiting for micrel's reply)
		 */
		bmcr &= ~(BMCR_FULLDPLX);
		err = phy_write(phydev, MII_BMCR, bmcr);
		if (err < 0)
			return err;
	}
	err = genphy_config_aneg(phydev);
	return err;
}

static struct phy_driver ksphy_driver[] = {
{
	.phy_id		= PHY_ID_KS8737,
	.phy_id_mask	= 0x00fffff0,
	.name		= "Micrel KS8737",
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause),
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= kszphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= kszphy_ack_interrupt,
	.config_intr	= ks8737_config_intr,
	.driver		= { .owner = THIS_MODULE,},
}, {
	.phy_id		= PHY_ID_KS8041,
	.phy_id_mask	= 0x00fffff0,
	.name		= "Micrel KS8041",
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= kszphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= kszphy_ack_interrupt,
	.config_intr	= kszphy_config_intr,
	.driver		= { .owner = THIS_MODULE,},
}, {
	.phy_id		= PHY_ID_KS8051,
	.phy_id_mask	= 0x00fffff0,
	.name		= "Micrel KS8051",
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= ksz8051_config_init,
	.config_aneg	= ksz8051_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= kszphy_ack_interrupt,
	.config_intr	= kszphy_config_intr,
	.driver		= { .owner = THIS_MODULE,},
}, {
	.phy_id		= PHY_ID_KS8081, /* same ase 8051(EOL) */
	.phy_id_mask	= 0x00fffff0,
	.name		= "Micrel KS8081",
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= ksz8051_config_init,
	.config_aneg	= ksz8051_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= kszphy_ack_interrupt,
	.config_intr	= kszphy_config_intr,
	.driver		= { .owner = THIS_MODULE,},
}, {
	.phy_id		= PHY_ID_KS8001,
	.name		= "Micrel KS8001 or KS8721",
	.phy_id_mask	= 0x00ffffff,
	.features	= (PHY_BASIC_FEATURES | SUPPORTED_Pause),
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= kszphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= kszphy_ack_interrupt,
	.config_intr	= kszphy_config_intr,
	.driver		= { .owner = THIS_MODULE,},
}, {
	.phy_id		= PHY_ID_KSZ9021,
	.phy_id_mask	= 0x000ffffe,
	.name		= "Micrel KSZ9021 Gigabit PHY",
	.features	= (PHY_GBIT_FEATURES | SUPPORTED_Pause
				| SUPPORTED_Asym_Pause),
	.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
	.config_init	= kszphy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.ack_interrupt	= kszphy_ack_interrupt,
	.config_intr	= ksz9021_config_intr,
	.driver		= { .owner = THIS_MODULE, },
} };

static int __init ksphy_init(void)
{
	return phy_drivers_register(ksphy_driver,
		ARRAY_SIZE(ksphy_driver));
}

static void __exit ksphy_exit(void)
{
	phy_drivers_unregister(ksphy_driver,
		ARRAY_SIZE(ksphy_driver));
}

module_init(ksphy_init);
module_exit(ksphy_exit);

MODULE_DESCRIPTION("Micrel PHY driver");
MODULE_AUTHOR("David J. Choi");
MODULE_LICENSE("GPL");
/*
static struct mdio_device_id __maybe_unused micrel_tbl[] = {
	{ PHY_ID_KSZ9021, 0x000ffffe },
	{ PHY_ID_KS8001, 0x00ffffff },
	{ PHY_ID_KS8737, 0x00fffff0 },
	{ PHY_ID_KS8041, 0x00fffff0 },
	{ PHY_ID_KS8051, 0x00fffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, micrel_tbl);
*/

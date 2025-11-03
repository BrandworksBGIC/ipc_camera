/******************************************************************************
 *
 * Copyright(c) 2007 - 2017 Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 *****************************************************************************/
#ifndef __RTL8830A_LED_H__
#define __RTL8830A_LED_H__

#ifdef CONFIG_RTW_SW_LED

/* ********************************************************************************
 * Interface to manipulate LED objects.
 * ******************************************************************************** */
#ifdef CONFIG_USB_HCI
	void rtl8830au_InitSwLeds(PADAPTER padapter);
	void rtl8830au_DeInitSwLeds(PADAPTER padapter);
#endif
#ifdef CONFIG_PCI_HCI
	void rtl8830ae_InitSwLeds(PADAPTER padapter);
	void rtl8830ae_DeInitSwLeds(PADAPTER padapter);
#endif
#if defined(CONFIG_SDIO_HCI) || defined(CONFIG_GSPI_HCI)
	void rtl8830as_InitSwLeds(PADAPTER padapter);
	void rtl8830as_DeInitSwLeds(PADAPTER padapter);
#endif

#endif
#endif /*CONFIG_RTW_SW_LED*/

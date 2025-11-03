/* SPDX-License-Identifier: GPL-2.0-only
 *
 *  Copyright (C) 2021 Realtek Semiconductor Corp.
 *  All Rights Reserved
 */
#ifndef __DRIVERS_USB_REALTEK_REGS_H__
#define __DRIVERS_USB_REALTEK_REGS_H__

/* usb phy clk register */
#define USBPHY_CLK_CFG_REG 0x188d004c

/* ep0 mc register */
#define R_EP0_MC_BUF_CTL 0x0000
#define R_EP0_MC_BUF_BC 0x0004
#define R_MC_SID 0x0008
#define R_MC_DUMMY0 0x000C
#define R_MC_DEV_CFG 0x0010
#define EP0_BASE 0x1000

/* R_EP0_MC_BUF_CTL 0x0000 */
#define U_BUF0_EP0_RX_EN_OFFSET 0
#define U_BUF0_EP0_RX_EN_BITS 1
#define U_BUF0_EP0_RX_EN_MASK (((1 << 1) - 1) << 0)
#define U_BUF0_EP0_TX_EN_OFFSET 1
#define U_BUF0_EP0_TX_EN_BITS 1
#define U_BUF0_EP0_TX_EN_MASK (((1 << 1) - 1) << 1)
/* R_EP0_MC_BUF_BC 0x0004 */
#define U_BUF0_TX_BC_OFFSET 0
#define U_BUF0_TX_BC_BITS 16
#define U_BUF0_TX_BC_MASK (((1 << 16) - 1) << 0)
#define U_BUF0_RX_BC_OFFSET 16
#define U_BUF0_RX_BC_BITS 16
#define U_BUF0_RX_BC_MASK (((1 << 16) - 1) << 16)
/* R_MC_SID 0x0008 */
#define U_BUF0_SID_OFFSET 0
#define U_BUF0_SID_BITS 16
#define U_BUF0_SID_MASK (((1 << 16) - 1) << 0)
#define U_FIFO_SID_OFFSET 16
#define U_FIFO_SID_BITS 16
#define U_FIFO_SID_MASK (((1 << 16) - 1) << 16)
/* R_MC_DUMMY0 0x000C */
/* R_MC_DEV_CFG 0x0010 */
#define CFG_U2DEV_SEL_OFFSET 0
#define CFG_U2DEV_SEL_BITS 1
#define CFG_U2DEV_SEL_MASK (((1 << 1) - 1) << 0)

/* usb register */
#define USB_CTRL 0x0000
#define USB_ADDR 0x0004
#define USB_IRQ_EN 0x0008
#define USB_IRQ_STATUS 0x0010
#define USB_FORCE_CMD 0x0014
#define USB_UTMI_CTRL 0x0018
#define USB_UTMI_CFG 0x001C
#define USB_UTMI_STAT 0x0020
#define USB_PHY_CTRL 0x0024
#define USB_DPHY_CFG 0x0028
#define USB_SLBTEST 0x0030
#define USB_PKERR_CNT 0x0034
#define USB_RXERR_CNT 0x0038
#define USB_SIE_STATUS 0x0040
#define USB_EP_CFG0 0x0048
#define USB_EP_CTL0 0x004C
#define USB_EP_CTL1 0x0050
#define USB_EP_MAXPKT0 0x0054
#define USB_EP0_SETUP_DATA0 0x0060
#define USB_EP0_SETUP_DATA1 0x0064
#define USB_LPM_CFG0 0x0068
#define USB_LPM_DUMMY 0x0070
#define USB_DUMMY0 0x0080
#define USB_DUMMY1 0x0084

/* USB_CTRL 0x0000 */
#define CONNECT_EN_OFFSET 0
#define CONNECT_EN_BITS 1
#define CONNECT_EN_MASK (((1 << 1) - 1) << 0)
#define WAKEUP_EN_OFFSET 1
#define WAKEUP_EN_BITS 1
#define WAKEUP_EN_MASK (((1 << 1) - 1) << 1)
#define SUSPND_EN_OFFSET 2
#define SUSPND_EN_BITS 1
#define SUSPND_EN_MASK (((1 << 1) - 1) << 2)
#define CFG_FORCE_FS_JMP_SPD_NEG_FS_OFFSET 3
#define CFG_FORCE_FS_JMP_SPD_NEG_FS_BITS 1
#define CFG_FORCE_FS_JMP_SPD_NEG_FS_MASK (((1 << 1) - 1) << 3)
#define MODE_HS_OFFSET 4
#define MODE_HS_BITS 1
#define MODE_HS_MASK (((1 << 1) - 1) << 4)
#define CFG_FORCE_FW_SUSPEND_OFFSET 5
#define CFG_FORCE_FW_SUSPEND_BITS 1
#define CFG_FORCE_FW_SUSPEND_MASK (((1 << 1) - 1) << 5)
#define CFG_FORCE_FW_REMOTE_WAKEUP_OFFSET 6
#define CFG_FORCE_FW_REMOTE_WAKEUP_BITS 1
#define CFG_FORCE_FW_REMOTE_WAKEUP_MASK (((1 << 1) - 1) << 6)
#define CFG_SOF_INTERVAL_OFFSET 8
#define CFG_SOF_INTERVAL_BITS 8
#define CFG_SOF_INTERVAL_MASK (((1 << 8) - 1) << 8)
#define SOF_CNT_EN_OFFSET 16
#define SOF_CNT_EN_BITS 1
#define SOF_CNT_EN_MASK (((1 << 1) - 1) << 16)
/* USB_ADDR 0x0004 */
#define DEVADDR_SET_OFFSET 0
#define DEVADDR_SET_BITS 7
#define DEVADDR_SET_MASK (((1 << 7) - 1) << 0)
#define FORCE_DEVADDR_OFFSET 7
#define FORCE_DEVADDR_BITS 1
#define FORCE_DEVADDR_MASK (((1 << 1) - 1) << 7)
#define DEVADDR_OFFSET 8
#define DEVADDR_BITS 7
#define DEVADDR_MASK (((1 << 7) - 1) << 8)
/* USB_IRQ_EN 0x0008 */
#define IE_LS_OFFSET 0
#define IE_LS_BITS 1
#define IE_LS_MASK (((1 << 1) - 1) << 0)
#define IE_SOF_OFFSET 1
#define IE_SOF_BITS 1
#define IE_SOF_MASK (((1 << 1) - 1) << 1)
#define IE_SUSPND_OFFSET 2
#define IE_SUSPND_BITS 1
#define IE_SUSPND_MASK (((1 << 1) - 1) << 2)
#define IE_RESUME_OFFSET 3
#define IE_RESUME_BITS 1
#define IE_RESUME_MASK (((1 << 1) - 1) << 3)
#define IE_SE0RST_OFFSET 4
#define IE_SE0RST_BITS 1
#define IE_SE0RST_MASK (((1 << 1) - 1) << 4)
#define IE_L1SLEEP_OFFSET 5
#define IE_L1SLEEP_BITS 1
#define IE_L1SLEEP_MASK (((1 << 1) - 1) << 5)
#define IE_L1RESUME_OFFSET 6
#define IE_L1RESUME_BITS 1
#define IE_L1RESUME_MASK (((1 << 1) - 1) << 6)
#define IE_EP0INT_OFFSET 7
#define IE_EP0INT_BITS 1
#define IE_EP0INT_MASK (((1 << 1) - 1) << 7)
#define IE_EP0OUTT_OFFSET 8
#define IE_EP0OUTT_BITS 1
#define IE_EP0OUTT_MASK (((1 << 1) - 1) << 8)
#define IE_EP0IN_OFFSET 9
#define IE_EP0IN_BITS 1
#define IE_EP0IN_MASK (((1 << 1) - 1) << 9)
#define IE_EP0OUT_OFFSET 10
#define IE_EP0OUT_BITS 1
#define IE_EP0OUT_MASK (((1 << 1) - 1) << 10)
#define IE_EP0OSHT_OFFSET 11
#define IE_EP0OSHT_BITS 1
#define IE_EP0OSHT_MASK (((1 << 1) - 1) << 11)
#define IE_EP0CSEND_OFFSET 12
#define IE_EP0CSEND_BITS 1
#define IE_EP0CSEND_MASK (((1 << 1) - 1) << 12)
#define IE_SETUP_OFFSET 13
#define IE_SETUP_BITS 1
#define IE_SETUP_MASK (((1 << 1) - 1) << 13)
#define IE_EP0CS_OFFSET 14
#define IE_EP0CS_BITS 1
#define IE_EP0CS_MASK (((1 << 1) - 1) << 14)
#define IE_SOF_INTERVAL_OFFSET 15
#define IE_SOF_INTERVAL_BITS 1
#define IE_SOF_INTERVAL_MASK (((1 << 1) - 1) << 15)
/* USB_IRQ_STATUS 0x0010 */
#define I_LSF_OFFSET 0
#define I_LSF_BITS 1
#define I_LSF_MASK (((1 << 1) - 1) << 0)
#define I_SOFF_OFFSET 1
#define I_SOFF_BITS 1
#define I_SOFF_MASK (((1 << 1) - 1) << 1)
#define I_SUSPNDF_OFFSET 2
#define I_SUSPNDF_BITS 1
#define I_SUSPNDF_MASK (((1 << 1) - 1) << 2)
#define I_RESUMEF_OFFSET 3
#define I_RESUMEF_BITS 1
#define I_RESUMEF_MASK (((1 << 1) - 1) << 3)
#define I_SE0RSTF_OFFSET 4
#define I_SE0RSTF_BITS 1
#define I_SE0RSTF_MASK (((1 << 1) - 1) << 4)
#define I_L1SLEEPF_OFFSET 5
#define I_L1SLEEPF_BITS 1
#define I_L1SLEEPF_MASK (((1 << 1) - 1) << 5)
#define I_L1RESUMEF_OFFSET 6
#define I_L1RESUMEF_BITS 1
#define I_L1RESUMEF_MASK (((1 << 1) - 1) << 6)
#define I_EP0INTF_OFFSET 7
#define I_EP0INTF_BITS 1
#define I_EP0INTF_MASK (((1 << 1) - 1) << 7)
#define I_EP0OUTTF_OFFSET 8
#define I_EP0OUTTF_BITS 1
#define I_EP0OUTTF_MASK (((1 << 1) - 1) << 8)
#define I_EP0INF_OFFSET 9
#define I_EP0INF_BITS 1
#define I_EP0INF_MASK (((1 << 1) - 1) << 9)
#define I_EP0OUTF_OFFSET 10
#define I_EP0OUTF_BITS 1
#define I_EP0OUTF_MASK (((1 << 1) - 1) << 10)
#define I_EP0OSHTF_OFFSET 11
#define I_EP0OSHTF_BITS 1
#define I_EP0OSHTF_MASK (((1 << 1) - 1) << 11)
#define I_EP0CSENDF_OFFSET 12
#define I_EP0CSENDF_BITS 1
#define I_EP0CSENDF_MASK (((1 << 1) - 1) << 12)
#define I_SETUPF_OFFSET 13
#define I_SETUPF_BITS 1
#define I_SETUPF_MASK (((1 << 1) - 1) << 13)
#define I_EP0CSF_OFFSET 14
#define I_EP0CSF_BITS 1
#define I_EP0CSF_MASK (((1 << 1) - 1) << 14)
#define I_SOF_INTERVAL_OFFSET 15
#define I_SOF_INTERVAL_BITS 1
#define I_SOF_INTERVAL_MASK (((1 << 1) - 1) << 15)
/* USB_FORCE_CMD 0x0014 */
#define FORCE_UTMI_RST_OFFSET 0
#define FORCE_UTMI_RST_BITS 1
#define FORCE_UTMI_RST_MASK (((1 << 1) - 1) << 0)
#define FORCE_PCE_RST_OFFSET 1
#define FORCE_PCE_RST_BITS 1
#define FORCE_PCE_RST_MASK (((1 << 1) - 1) << 1)
#define FORCE_PA_RST_OFFSET 2
#define FORCE_PA_RST_BITS 1
#define FORCE_PA_RST_MASK (((1 << 1) - 1) << 2)
#define FORCE_PCE_CMD_OFFSET 3
#define FORCE_PCE_CMD_BITS 1
#define FORCE_PCE_CMD_MASK (((1 << 1) - 1) << 3)
/* USB_UTMI_CTRL 0x0018 */
#define FORCE_DBSN_OFFSET 0
#define FORCE_DBSN_BITS 1
#define FORCE_DBSN_MASK (((1 << 1) - 1) << 0)
#define FORCE_NORM_OFFSET 1
#define FORCE_NORM_BITS 1
#define FORCE_NORM_MASK (((1 << 1) - 1) << 1)
#define FORCE_FSTERM_OFFSET 2
#define FORCE_FSTERM_BITS 1
#define FORCE_FSTERM_MASK (((1 << 1) - 1) << 2)
#define FORCE_HSTERM_OFFSET 3
#define FORCE_HSTERM_BITS 1
#define FORCE_HSTERM_MASK (((1 << 1) - 1) << 3)
#define FORCE_FSXCVR_OFFSET 4
#define FORCE_FSXCVR_BITS 1
#define FORCE_FSXCVR_MASK (((1 << 1) - 1) << 4)
#define FORCE_HSXCVR_OFFSET 5
#define FORCE_HSXCVR_BITS 1
#define FORCE_HSXCVR_MASK (((1 << 1) - 1) << 5)
#define FORCE_FS_OFFSET 6
#define FORCE_FS_BITS 1
#define FORCE_FS_MASK (((1 << 1) - 1) << 6)
#define FORCE_HS_OFFSET 7
#define FORCE_HS_BITS 1
#define FORCE_HS_MASK (((1 << 1) - 1) << 7)
/* USB_UTMI_CFG 0x001C */
#define CFG_FAST_CHIRP_K_OFFSET 0
#define CFG_FAST_CHIRP_K_BITS 1
#define CFG_FAST_CHIRP_K_MASK (((1 << 1) - 1) << 0)
#define CFG_CHIRP_K_LENGTH_OFFSET 1
#define CFG_CHIRP_K_LENGTH_BITS 2
#define CFG_CHIRP_K_LENGTH_MASK (((1 << 2) - 1) << 1)
/* USB_UTMI_STAT 0x0020 */
#define UTMI_OPMODE_OFFSET 0
#define UTMI_OPMODE_BITS 2
#define UTMI_OPMODE_MASK (((1 << 2) - 1) << 0)
#define UTMI_TERMSEL_OFFSET 2
#define UTMI_TERMSEL_BITS 1
#define UTMI_TERMSEL_MASK (((1 << 1) - 1) << 2)
#define UTMI_XCVRSEL_OFFSET 3
#define UTMI_XCVRSEL_BITS 1
#define UTMI_XCVRSEL_MASK (((1 << 1) - 1) << 3)
#define UTMI_LINESTATE_OFFSET 4
#define UTMI_LINESTATE_BITS 2
#define UTMI_LINESTATE_MASK (((1 << 2) - 1) << 4)
/* USB_PHY_CTRL 0x0024 */
#define SOF_EN_OFFSET 0
#define SOF_EN_BITS 1
#define SOF_EN_MASK (((1 << 1) - 1) << 0)
#define FW_SEL_SERCV_OFFSET 1
#define FW_SEL_SERCV_BITS 1
#define FW_SEL_SERCV_MASK (((1 << 1) - 1) << 1)
#define RPU_FW_EN_OFFSET 2
#define RPU_FW_EN_BITS 1
#define RPU_FW_EN_MASK (((1 << 1) - 1) << 2)
#define SERCV_FW_EN_OFFSET 3
#define SERCV_FW_EN_BITS 1
#define SERCV_FW_EN_MASK (((1 << 1) - 1) << 3)
#define RPU_HIGH_OFFSET 4
#define RPU_HIGH_BITS 2
#define RPU_HIGH_MASK (((1 << 2) - 1) << 4)
#define RPU_LOW_OFFSET 6
#define RPU_LOW_BITS 2
#define RPU_LOW_MASK (((1 << 2) - 1) << 6)
/* USB_DPHY_CFG 0x0028 */
#define CLK60_NEGALIGN_OFFSET 0
#define CLK60_NEGALIGN_BITS 1
#define CLK60_NEGALIGN_MASK (((1 << 1) - 1) << 0)
#define LATE_DLLEN_OFFSET 1
#define LATE_DLLEN_BITS 1
#define LATE_DLLEN_MASK (((1 << 1) - 1) << 1)
#define FS_XCVR_POW_SAV_OFFSET 2
#define FS_XCVR_POW_SAV_BITS 1
#define FS_XCVR_POW_SAV_MASK (((1 << 1) - 1) << 2)
#define HS_ANA_TX_PWD_EN_OFFSET 3
#define HS_ANA_TX_PWD_EN_BITS 1
#define HS_ANA_TX_PWD_EN_MASK (((1 << 1) - 1) << 3)
#define HS_XMT_PWD_EN_OFFSET 4
#define HS_XMT_PWD_EN_BITS 1
#define HS_XMT_PWD_EN_MASK (((1 << 1) - 1) << 4)
#define CFG_RXACT_EARLY_OFFSET 5
#define CFG_RXACT_EARLY_BITS 1
#define CFG_RXACT_EARLY_MASK (((1 << 1) - 1) << 5)
#define FS_PHASE_SEL_OFFSET 8
#define FS_PHASE_SEL_BITS 4
#define FS_PHASE_SEL_MASK (((1 << 4) - 1) << 8)
#define CFG_EB_DEPTH_OFFSET 12
#define CFG_EB_DEPTH_BITS 4
#define CFG_EB_DEPTH_MASK (((1 << 4) - 1) << 12)
#define USB2_PHY_DEBUG_ADDR_OFFSET 16
#define USB2_PHY_DEBUG_ADDR_BITS 3
#define USB2_PHY_DEBUG_ADDR_MASK (((1 << 3) - 1) << 16)
/* USB_SLBTEST 0x0030 */
#define SLB_SEED_OFFSET 0
#define SLB_SEED_BITS 8
#define SLB_SEED_MASK (((1 << 8) - 1) << 0)
#define SLB_PSL_OFFSET 8
#define SLB_PSL_BITS 2
#define SLB_PSL_MASK (((1 << 2) - 1) << 8)
#define SLB_DONE_OFFSET 10
#define SLB_DONE_BITS 1
#define SLB_DONE_MASK (((1 << 1) - 1) << 10)
#define SLB_FAIL_OFFSET 11
#define SLB_FAIL_BITS 1
#define SLB_FAIL_MASK (((1 << 1) - 1) << 11)
#define SLB_RST_OFFSET 12
#define SLB_RST_BITS 1
#define SLB_RST_MASK (((1 << 1) - 1) << 12)
#define SLB_EN_OFFSET 13
#define SLB_EN_BITS 1
#define SLB_EN_MASK (((1 << 1) - 1) << 13)
#define USBTMOD_OFFSET 16
#define USBTMOD_BITS 3
#define USBTMOD_MASK (((1 << 3) - 1) << 16)
/* USB_PKERR_CNT 0x0034 */
#define PKTERR_CNT1_OFFSET 0
#define PKTERR_CNT1_BITS 8
#define PKTERR_CNT1_MASK (((1 << 8) - 1) << 0)
#define PKTERR_CNT2_OFFSET 8
#define PKTERR_CNT2_BITS 8
#define PKTERR_CNT2_MASK (((1 << 8) - 1) << 8)
#define PKTERR_CLR_OFFSET 16
#define PKTERR_CLR_BITS 1
#define PKTERR_CLR_MASK (((1 << 1) - 1) << 16)
/* USB_RXERR_CNT 0x0038 */
#define RXERR_CNT1_OFFSET 0
#define RXERR_CNT1_BITS 8
#define RXERR_CNT1_MASK (((1 << 8) - 1) << 0)
#define RXERR_CNT2_OFFSET 8
#define RXERR_CNT2_BITS 8
#define RXERR_CNT2_MASK (((1 << 8) - 1) << 8)
#define RXERR_CLR_OFFSET 16
#define RXERR_CLR_BITS 1
#define RXERR_CLR_MASK (((1 << 1) - 1) << 16)
/* USB_SIE_STATUS 0x0040 */
// #define MODE_HS_OFFSET 0
// #define MODE_HS_BITS 1
// #define MODE_HS_MASK (((1 << 1) - 1) << 0)
// #define CFG_FORCE_FS_JMP_SPD_NEG_FS_OFFSET 1
// #define CFG_FORCE_FS_JMP_SPD_NEG_FS_BITS 1
// #define CFG_FORCE_FS_JMP_SPD_NEG_FS_MASK (((1 << 1) - 1) << 1)
// #define FORCE_FS_OFFSET 2
// #define FORCE_FS_BITS 1
// #define FORCE_FS_MASK (((1 << 1) - 1) << 2)
/* USB_EP_CFG0 0x0048 */
#define EP0_NAKOUT_MODE_OFFSET 0
#define EP0_NAKOUT_MODE_BITS 1
#define EP0_NAKOUT_MODE_MASK (((1 << 1) - 1) << 0)
#define CFG_LFOE_LFVO_GLITCH_OFFSET 6
#define CFG_LFOE_LFVO_GLITCH_BITS 1
#define CFG_LFOE_LFVO_GLITCH_MASK (((1 << 1) - 1) << 6)
#define CFG_LFSE0_LFVO_GLITCH_OFFSET 7
#define CFG_LFSE0_LFVO_GLITCH_BITS 1
#define CFG_LFSE0_LFVO_GLITCH_MASK (((1 << 1) - 1) << 7)
/* USB_EP_CTL0 0x004C */
#define EP0_DIR_OFFSET 0
#define EP0_DIR_BITS 1
#define EP0_DIR_MASK (((1 << 1) - 1) << 0)
#define EP0_STALL_OFFSET 1
#define EP0_STALL_BITS 1
#define EP0_STALL_MASK (((1 << 1) - 1) << 1)
#define EP0_RESET_OFFSET 2
#define EP0_RESET_BITS 1
#define EP0_RESET_MASK (((1 << 1) - 1) << 2)
#define EP0_CONTROL_STAGE_OFFSET 3
#define EP0_CONTROL_STAGE_BITS 2
#define EP0_CONTROL_STAGE_MASK (((1 << 2) - 1) << 3)
/* USB_EP_CTL1 0x0050 */
#define EP0_CSH_OFFSET 0
#define EP0_CSH_BITS 1
#define EP0_CSH_MASK (((1 << 1) - 1) << 0)
/* USB_EP_MAXPKT0 0x0054 */
#define EP0_MAXPKT_OFFSET 0
#define EP0_MAXPKT_BITS 7
#define EP0_MAXPKT_MASK (((1 << 7) - 1) << 0)
/* USB_EP0_SETUP_DATA0 0x0060 */
/* USB_EP0_SETUP_DATA1 0x0064 */
/* USB_LPM_CFG0 0x0068 */
#define LPM_HIRD_SAVE_OFFSET 0
#define LPM_HIRD_SAVE_BITS 4
#define LPM_HIRD_SAVE_MASK (((1 << 4) - 1) << 0)
#define CFG_LPM_READY_OFFSET 4
#define CFG_LPM_READY_BITS 1
#define CFG_LPM_READY_MASK (((1 << 1) - 1) << 4)
#define U_LPM_AUTO_OFFSET 5
#define U_LPM_AUTO_BITS 1
#define U_LPM_AUTO_MASK (((1 << 1) - 1) << 5)
#define CFG_LPM_SUPPORT_OFFSET 6
#define CFG_LPM_SUPPORT_BITS 1
#define CFG_LPM_SUPPORT_MASK (((1 << 1) - 1) << 6)
#define LPM_AUTO_EN_OFFSET 7
#define LPM_AUTO_EN_BITS 1
#define LPM_AUTO_EN_MASK (((1 << 1) - 1) << 7)
#define U_FORCE_LPM_L1_RESUME_OFFSET 8
#define U_FORCE_LPM_L1_RESUME_BITS 1
#define U_FORCE_LPM_L1_RESUME_MASK (((1 << 1) - 1) << 8)
#define U_FRC_EP0_BUSY_REJECT_LPM_OFFSET 9
#define U_FRC_EP0_BUSY_REJECT_LPM_BITS 1
#define U_FRC_EP0_BUSY_REJECT_LPM_MASK (((1 << 1) - 1) << 9)
#define U_HOST_REMOTEWAKEUP_VALID_OFFSET 13
#define U_HOST_REMOTEWAKEUP_VALID_BITS 1
#define U_HOST_REMOTEWAKEUP_VALID_MASK (((1 << 1) - 1) << 13)
/* USB_LPM_DUMMY 0x0070 */
#define U_LPM_DMY0_OFFSET 0
#define U_LPM_DMY0_BITS 8
#define U_LPM_DMY0_MASK (((1 << 8) - 1) << 0)
#define U_LPM_DMY1_OFFSET 8
#define U_LPM_DMY1_BITS 8
#define U_LPM_DMY1_MASK (((1 << 8) - 1) << 8)
#define U_LPM_DMY2_OFFSET 16
#define U_LPM_DMY2_BITS 8
#define U_LPM_DMY2_MASK (((1 << 8) - 1) << 16)
#define U_LPM_DMY3_OFFSET 24
#define U_LPM_DMY3_BITS 8
#define U_LPM_DMY3_MASK (((1 << 8) - 1) << 24)
/* USB_DUMMY0 0x0080 */
#define SIE_DUMMY0_OFFSET 0
#define SIE_DUMMY0_BITS 8
#define SIE_DUMMY0_MASK (((1 << 8) - 1) << 0)
#define SIE_DUMMY1_OFFSET 8
#define SIE_DUMMY1_BITS 8
#define SIE_DUMMY1_MASK (((1 << 8) - 1) << 8)
#define SIE_DUMMY2_OFFSET 16
#define SIE_DUMMY2_BITS 8
#define SIE_DUMMY2_MASK (((1 << 8) - 1) << 16)
#define SIE_DUMMY3_OFFSET 24
#define SIE_DUMMY3_BITS 8
#define SIE_DUMMY3_MASK (((1 << 8) - 1) << 24)
/* USB_DUMMY1 0x0084 */
#define SIE_DUMMY4_OFFSET 0
#define SIE_DUMMY4_BITS 8
#define SIE_DUMMY4_MASK (((1 << 8) - 1) << 0)
#define SIE_DUMMY5_OFFSET 8
#define SIE_DUMMY5_BITS 8
#define SIE_DUMMY5_MASK (((1 << 8) - 1) << 8)
#define SIE_DUMMY6_OFFSET 16
#define SIE_DUMMY6_BITS 8
#define SIE_DUMMY6_MASK (((1 << 8) - 1) << 16)
#define SIE_DUMMY7_OFFSET 24
#define SIE_DUMMY7_BITS 8
#define SIE_DUMMY7_MASK (((1 << 8) - 1) << 24)
#endif /* __DRIVERS_USB_REALTEK_REGS_H__ */

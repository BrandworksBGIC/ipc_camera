// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * THIS SOFTWARE IS CONFIDENTIAL AND PROPRIETARY TO REALTEK SEMICONDUCTOR
 * CORP. DISCLOSURE, REPRODUCTION, REDISTRIBUTION, IN WHOLE OR IN PART, OF
 * THIS WORK AND ITS DERIVATIVES WITHOUT EXPRESS PERMISSION IS PROHIBITED.
 *
 * REALTEK SEMICONDUCTOR CORP. RESERVES THE RIGHT TO UPDATE, MODIFY, OR
 * DISCONTINUE THIS SOFTWARE AT ANY TIME WITHOUT NOTICE. THIS SOFTWARE IS
 * PROVIDED BY THE REGENTS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include "pm-rts3917.h"
/*
 * Below offsets are based on struct rts_pm_dev_info
 * which defined in arch/arm/mach-realtek/pm.c, this
 * structure contains necessary pm info for low level
 * suspend related code.
 */
#define PM_INFO_RTS_SRAM_P_OFFSET		0x0
#define PM_INFO_RTS_SRAM_V_OFFSET		0x4
#define PM_INFO_RTS_DDRC_P_OFFSET		0x8
#define PM_INFO_RTS_DDRC_V_OFFSET		0xC
#define PM_INFO_RTS_DDRP_P_OFFSET		0x10
#define PM_INFO_RTS_DDRP_V_OFFSET		0x14
#define PM_INFO_RTS_MISC_P_OFFSET		0x18
#define PM_INFO_RTS_MISC_V_OFFSET		0x1C
#define PM_INFO_RTS_GPLL_P_OFFSET		0x20
#define PM_INFO_RTS_GPLL_V_OFFSET		0x24
#define PM_INFO_RTS_CLK_P_OFFSET		0x28
#define PM_INFO_RTS_CLK_V_OFFSET		0x2C
#define PM_INFO_RTS_WDT_P_OFFSET		0x30
#define PM_INFO_RTS_WDT_V_OFFSET		0x34
#define PM_INFO_RTS_WDT_FLAG_OFFSET		0x38

	.align 3

ENTRY(rts_suspend)
	/* Save registers on stack */
	push	{r0 - r12, lr}

	/*
	 * make sure TLB contain the addr we want,
	 * as we will access them after putting ddr in self refresh.
	 */

	ldr	r11, [r0, #PM_INFO_RTS_SRAM_V_OFFSET]
	ldr	r7, [r11, #0x0]
	ldr	r11, [r0, #PM_INFO_RTS_DDRC_V_OFFSET]
	ldr	r7, [r11, #0x0]
	ldr	r11, [r0, #PM_INFO_RTS_DDRP_V_OFFSET]
	ldr	r7, [r11, #0x0]
	ldr	r11, [r0, #PM_INFO_RTS_MISC_V_OFFSET]
	ldr	r7, [r11, #0x0]
	ldr	r11, [r0, #PM_INFO_RTS_GPLL_V_OFFSET]
	ldr	r7, [r11, #0x0]
	ldr	r11, [r0, #PM_INFO_RTS_CLK_V_OFFSET]
	ldr	r7, [r11, #0x0]
	ldr	r11, [r0, #PM_INFO_RTS_WDT_V_OFFSET]
	ldr	r7, [r11, #0x0]

	/* initial retry count, retry 4 - 1 times */
	ldr	r3, =0x4
sref_retry:
	subs	r3, r3, #0x1
	beq	exit_sref

	/* start to put DDR into self-refresh */
	ldr	r11, [r0, #PM_INFO_RTS_DDRC_V_OFFSET]

	/* send PREA cmd to put ddr in idle status*/
	ldr	r7, =0x601
	str	r7, [r11, #DDRC_CSR_REG]

	ldr	r7, =0x00400400
	str	r7, [r11, #DDRC_CMD_DPIN_REG]
	ldr	r7, =0x8
	str	r7, [r11, #DDRC_CCR_REG]
prea:
	ldr	r7, [r11, #DDRC_CCR_REG]
	ands	r7, r7, #0x8
	beq	prea

	ldr	r7, =0x600
	str	r7, [r11, #DDRC_CSR_REG]

	/* 1. set dynamic CKE: CR_IOCR[2] = 0 */
	ldr	r7, [r11, #DDRC_IOCR_REG]
	bic	r7, r7, #0x4
	str	r7, [r11, #DDRC_IOCR_REG]

	/* 2. set REF enable: CR_DRR[28] = 0 */
	ldr	r7, [r11, #DDRC_DRR_REG]
	bic	r7, r7, #0x10000000
	str	r7, [r11, #DDRC_DRR_REG]

	/* 3. set tPD_EN: CR_TPR0[23] = 1 */
	ldr	r7, [r11, #DDRC_TPR0_REG]
	orr	r7, r7, #0x00800000
	str	r7, [r11, #DDRC_TPR0_REG]

	/* 4. set dpin-SRE command: CR_CDPIN / CR_CDPIN_NDGE */
	ldr	r7, =0x00200000
	str	r7, [r11, #DDRC_CMD_DPIN_REG]
	str	r7, [r11, #DDRC_CMD_DPIN_NDGE_REG]

	/* 5. set SREX start: set CR_CCR[4] = 1 */
	ldr	r7, =0x10
	str	r7, [r11, #DDRC_CCR_REG]

	/* 6. check CCR[4], CCR[4] = 0 means auto SREX is active */
	ldr	r7, [r11, #DDRC_CCR_REG]
	ands	r7, r7, #0x10
	bne	sref_retry

	/* check CSR[4] SRE_ST = 1 */
	/* ensure SRE command has been issued */
wait_sre_cmd:
	/* check CCR[4], CCR[4] = 1 means auto SREX exited, then try again */
	ldr	r7, [r11, #DDRC_CCR_REG]
	ands	r7, r7, #0x10
	bne	sref_retry

	ldr	r7, [r11, #DDRC_CSR_REG]
	ands	r7, r7, #0x10
	beq	wait_sre_cmd

	/* delay to ensure the SRE cmd has been issued */
	ldr	r7, =10
loop1:
	subs	r7, r7, #0x1
	bne	loop1

	/* set PWDPAD15N bit[11] = 0: pad power down*/
	/* ldr	r11, [r0, #PM_INFO_RTS_MISC_V_OFFSET]
	 * ldr	r7, [r11, #SYS_DDR_PHYCFG_REG]
	 * bic	r7, r7, #0x800
	 * str	r7, [r11, #SYS_DDR_PHYCFG_REG]
	 */

	/* set pad power down: */
	ldr	r11, [r0, #PM_INFO_RTS_DDRP_V_OFFSET]

	/* zq pad power down*/
	ldr	r7, [r11, #DDRP_PAD_BUS_1_REG]
	ldr	r6, =ZQ_PD_BIT
	orr	r7, r7, r6
	str	r7, [r11, #DDRP_PAD_BUS_1_REG]

	/* dq/dqs/dm pad power down */
	ldr	r7, [r11, #DDRP_READ_DBG_CTRL_SET0_REG]
	ldr	r6, =DQS_DQ_DM_PD
	orr	r7, r7, r6
	str	r7, [r11, #DDRP_READ_DBG_CTRL_SET0_REG]

	ldr	r7, [r11, #DDRP_READ_DBG_CTRL_SET1_REG]
	ldr	r6, =DQS_DQ_DM_PD
	orr	r7, r7, r6
	str	r7, [r11, #DDRP_READ_DBG_CTRL_SET1_REG]

	/* ck pad power down */
	ldr	r7, [r11, #DDRP_PAD_BUS_0_REG]
	ldr	r6, =CK_PD_BIT
	orr	r7, r7, r6
	str	r7, [r11, #DDRP_PAD_BUS_0_REG]

	/* tx pad oe disable */
	ldr	r7, =PAD_OE_CTL_DIS
	str	r7, [r11, #DDRP_PAD_OE_CTL_REG]
	ldr	r7, =PAD_OE_CTL_EXT_DIS
	str	r7, [r11, #DDRP_PAD_OE_CTL_EX_REG]

	/* close ddr phy clock */
	ldr	r11, [r0, #PM_INFO_RTS_DDRP_V_OFFSET]

	ldr	r7, [r11, #DDRP_PLL_CTL0_REG]
	ldr	r6, =0xfffff000
	and	r7, r7, r6
	str	r7, [r11, #DDRP_PLL_CTL0_REG]

	ldr	r7, [r11, #DDRP_PLL_CTL1_REG]
	bic	r7, r7, #0x30000
	str	r7, [r11, #DDRP_PLL_CTL1_REG]

	/* set ddr phy pll ldo reset DDRP_CRT_RST_CTL_REG[0] */
	ldr	r7, [r11, #DDRP_CRT_RST_CTL_REG]
	bic	r7, r7, #0x1
	str	r7, [r11, #DDRP_CRT_RST_CTL_REG]

	/* change CPU clock source to usbpll */
	ldr	r11, [r0, #PM_INFO_RTS_CLK_V_OFFSET]
	ldr	r7, =0x20
	str	r7, [r11, #CLK_CHANGE_REG]
	ldr	r1, [r11, #CPU_CLK_CFG_REG]	/* store cpu clk cfg in r1 */
	ldr	r7, =0x0100000c			/* USP_PLL / 1 / div6  */
	str	r7, [r11, #CPU_CLK_CFG_REG]
	ldr	r7, =0x0
	str	r7, [r11, #CLK_CHANGE_REG]

	/* change dram(p2bus) clock source to usbpll */
	ldr	r11, [r0, #PM_INFO_RTS_CLK_V_OFFSET]
	ldr	r7, =0x10
	str	r7, [r11, #CLK_CHANGE_REG]
	ldr	r2, [r11, #DRAM_CLK_CFG_REG]	/* store dram clk cfg in r2 */
	ldr	r7, =0x01000004			/* USB_PLL / 2 / div2 */
	str	r7, [r11, #DRAM_CLK_CFG_REG]
	ldr	r7, =0x0
	str	r7, [r11, #CLK_CHANGE_REG]

	/* close gpll0~2 */
	ldr	r11, [r0, #PM_INFO_RTS_GPLL_V_OFFSET]
	add	r11, r11, #GPLL0_OFFSET
	ldr	r7, =0x0
	str	r7, [r11, #GPLL_CTRL_REG]

	ldr	r11, [r0, #PM_INFO_RTS_GPLL_V_OFFSET]
	add	r11, r11, #GPLL1_OFFSET
	ldr	r7, =0x0
	str	r7, [r11, #GPLL_CTRL_REG]

	ldr	r11, [r0, #PM_INFO_RTS_GPLL_V_OFFSET]
	add	r11, r11, #GPLL2_OFFSET
	ldr	r7, =0x0
	str	r7, [r11, #GPLL_CTRL_REG]

	/* Zzz, enter stop mode */
	wfi

	/* turn on gpll0~2 */
	ldr	r11, [r0, #PM_INFO_RTS_GPLL_V_OFFSET]
	add	r11, r11, #GPLL0_OFFSET
	ldr	r7, =0x2
	str	r7, [r11, #GPLL_CTRL_REG]
	ldr	r7, =0x3
	str	r7, [r11, #GPLL_CTRL_REG]

	ldr	r11, [r0, #PM_INFO_RTS_GPLL_V_OFFSET]
	add	r11, r11, #GPLL1_OFFSET
	ldr	r7, =0x2
	str	r7, [r11, #GPLL_CTRL_REG]
	ldr	r7, =0x3
	str	r7, [r11, #GPLL_CTRL_REG]

	ldr	r11, [r0, #PM_INFO_RTS_GPLL_V_OFFSET]
	add	r11, r11, #GPLL2_OFFSET
	ldr	r7, =0x2
	str	r7, [r11, #GPLL_CTRL_REG]
	ldr	r7, =0x3
	str	r7, [r11, #GPLL_CTRL_REG]

	/* restore cpu clk */
	ldr	r11, [r0, #PM_INFO_RTS_CLK_V_OFFSET]
	ldr	r7, =0x20
	str	r7, [r11, #CLK_CHANGE_REG]
	str	r1, [r11, #CPU_CLK_CFG_REG] /* restore from r1 */
	ldr	r7, =0x0
	str	r7, [r11, #CLK_CHANGE_REG]

	/* restore dram(p2bus) clk */
	ldr	r11, [r0, #PM_INFO_RTS_CLK_V_OFFSET]
	ldr	r7, =0x10
	str	r7, [r11, #CLK_CHANGE_REG]
	str	r2, [r11, #DRAM_CLK_CFG_REG] /* restore from r2 */
	ldr	r7, =0x0
	str	r7, [r11, #CLK_CHANGE_REG]

	/* turn on ddr phy pll ldo*/
	ldr	r11, [r0, #PM_INFO_RTS_DDRP_V_OFFSET]

	ldr	r7, [r11, #DDRP_CRT_RST_CTL_REG]
	orr 	r7, r7, #0x1
	str	r7, [r11, #DDRP_CRT_RST_CTL_REG]

	/* turn on ddr phy clock */
	ldr	r7, [r11, #DDRP_PLL_CTL0_REG]
	ldr	r6, =0xfff
	orr	r7, r7, r6
	str	r7, [r11, #DDRP_PLL_CTL0_REG]

	ldr	r7, [r11, #DDRP_PLL_CTL1_REG]
	orr 	r7, r7, #0x30000
	str	r7, [r11, #DDRP_PLL_CTL1_REG]

	ldr	r11, [r0, #PM_INFO_RTS_MISC_V_OFFSET]
wait_pll:
	ldr	r7, [r11, #SYS_DDR_STATUS_REG]
	ands	r7, r7, #0x1
	beq	wait_pll

	/* set PWDPAD15N bit[11] = 1: disable pad power down*/
	/* ldr	r11, [r0, #PM_INFO_RTS_MISC_V_OFFSET]
	 * ldr	r7, [r11, #SYS_DDR_PHYCFG_REG]
	 * orr	r7, r7, #0x800
	 * str	r7, [r11, #SYS_DDR_PHYCFG_REG]
	 */

	/* set pad power on: */
	ldr	r11, [r0, #PM_INFO_RTS_DDRP_V_OFFSET]

	/* zq pad power on*/
	ldr	r7, [r11, #DDRP_PAD_BUS_1_REG]
	ldr	r6, =ZQ_PD_BIT
	bic	r7, r7, r6
	str	r7, [r11, #DDRP_PAD_BUS_1_REG]

	/* dq/dqs/dm pad power on */
	ldr	r7, [r11, #DDRP_READ_DBG_CTRL_SET0_REG]
	ldr	r6, =DQS_DQ_DM_PD
	bic	r7, r7, r6
	str	r7, [r11, #DDRP_READ_DBG_CTRL_SET0_REG]

	ldr	r7, [r11, #DDRP_READ_DBG_CTRL_SET1_REG]
	ldr	r6, =DQS_DQ_DM_PD
	bic	r7, r7, r6
	str	r7, [r11, #DDRP_READ_DBG_CTRL_SET1_REG]

	/* ck pad power on */
	ldr	r7, [r11, #DDRP_PAD_BUS_0_REG]
	ldr	r6, =CK_PD_BIT
	bic	r7, r7, r6
	str	r7, [r11, #DDRP_PAD_BUS_0_REG]

	/* tx pad oe enable */
	ldr	r7, =PAD_OE_CTL_EN
	str	r7, [r11, #DDRP_PAD_OE_CTL_REG]
	ldr	r7, =PAD_OE_CTL_EXT_EN
	str	r7, [r11, #DDRP_PAD_OE_CTL_EX_REG]

	/* wait to ensure pad power is stable */
	ldr	r7, =10000
loop2:
	subs	r7, r7, #0x1
	bne	loop2

	/* issue read cmds to exit self-refresh */
	ldr	r7, [lr]
	ldr	r6, =0x1000
	add	r6, r6, lr
	ldr	r7, [r6]

	/* check CCR[4], CCR[4] = 1 means auto SREX exited */
	ldr	r11, [r0, #PM_INFO_RTS_DDRC_V_OFFSET]
srx:	ldr	r7, [r11, #DDRC_CCR_REG]
	ands	r7, r7, #0x10
	beq	srx

#ifdef CONFIG_RTSX_WATCHDOG
	/* enable watchdog */
	ldr	r11, [r0, #PM_INFO_RTS_WDT_V_OFFSET]
	ldr	r7, [r11, #WATCHDOG_CFG_REG]
	ldr	r6, [r0, #PM_INFO_RTS_WDT_FLAG_OFFSET]
	orr	r7, r7, r6
	str	r7, [r11, #WATCHDOG_CFG_REG]
#endif

	/* return to suspend finish */
exit_sref:
	/* Restore registers, and return */
	pop	{r0 - r12, pc}
ENDPROC(rts_suspend)

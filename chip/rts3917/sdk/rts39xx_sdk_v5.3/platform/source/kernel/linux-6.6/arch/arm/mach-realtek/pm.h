/* SPDX-License-Identifier: GPL-2.0-only */
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

#ifndef _PM_H_
#define _PM_H_

struct rts_pm_base {
	phys_addr_t pbase;
	void __iomem *vbase;
};

/*
 * This structure is for passing necessary data for low level sram
 * suspend code(arch/arm/mach-realtek/suspend-rts.S), if this struct
 * definition is changed, the offset definition in
 * arch/arm/mach-realtek/suspend-rts.S must be also changed accordingly.
 */
struct rts_pm_dev_info {
	struct rts_pm_base sram_base;	/* sram base address */
	struct rts_pm_base ddrc_base;	/* ddr controller base address */
	struct rts_pm_base ddrp_base;	/* ddr phy base address */
	struct rts_pm_base misc_base;	/* misc base address */
	struct rts_pm_base gpll_base;	/* gpll base address */
	struct rts_pm_base clk_base;	/* clock base address */
	struct rts_pm_base wdt_base;	/* watchdog base address */
	unsigned int wdt_flag;		/* watchdog flag */
} __aligned(8);

extern void __iomem *suspend_sram_base;
extern void (*rts_suspend_in_sram_fn)(void __iomem *sram_vbase);

extern int rts_pm_suspend_to_ram_init(void);
extern void rts_pm_suspend_to_ram_unmap(void);

extern void rts_suspend(void __iomem *sram_vbase);

#endif /* _BSPCHIP_H_ */

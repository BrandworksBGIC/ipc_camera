/*
 * Realtek Semiconductor Corp.
 *
 * bsp/irq-ipi.c
 *     IPI initialization and handlers
 *
 * Copyright (C) 2006-2015 Tony Wu (tonywu@realtek.com)
 */
#if defined(CONFIG_SMP) \
    && !(defined(CONFIG_MIPS_CMP) || defined(CONFIG_MIPS_CPS))
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/interrupt.h>

/*
 * Handle SMP IPI interrupts
 *
 * Two IPI interrupts, resched and call, are handled here.
 */
static irqreturn_t plat_ipi_resched(int irq, void *devid)
{
	scheduler_ipi();
	return IRQ_HANDLED;
}

static irqreturn_t plat_ipi_call(int irq, void *devid)
{
	generic_smp_call_function_interrupt();
	return IRQ_HANDLED;
}

/*
 * Initialize IPI interrupts.
 *
 * In MIPS_MT_SMP mode, IPI interrupts are routed via SW0 and SW1.
 * When GIC is present, IPI interrupts are routed via GIC.
 */
static void __init plat_ipi_init(void)
{
#if defined(CONFIG_IRQ_GIC) || defined(CONFIG_MIPS_GIC)
	int irq;
	int cpu;

	/* setup GIC IPI interrupts */
	gic_setup_ipi(GIC_CPU_INT1, GIC_CPU_INT2);

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		irq = MIPS_GIC_IRQ_BASE + GIC_IPI_RESCHED(cpu);
		irq_set_handler(irq, handle_percpu_irq);
		if (request_irq(irq, plat_ipi_resched, IRQF_PERCPU, "IPI resched", NULL))
			panic("Can't request IPI resched interrupt");

		irq = MIPS_GIC_IRQ_BASE + GIC_IPI_CALL(cpu);
		irq_set_handler(irq, handle_percpu_irq);
		if (request_irq(irq, plat_ipi_call, IRQF_PERCPU, "IPI call", NULL))
			panic("Can't request IPI call interrupt");
	}
#elif defined(CONFIG_MIPS_MT_SMP)
	if (request_irq(0, plat_ipi_resched, IRQF_PERCPU, "IPI resched", NULL))
		panic("Can't request IPI0 interrupt");
	if (request_irq(1, plat_ipi_call, IRQF_PERCPU, "IPI call", NULL))
		panic("Can't request IPI1 interrupt");
#endif
}

#else
static void __init plat_ipi_init(void)
{
}
#endif

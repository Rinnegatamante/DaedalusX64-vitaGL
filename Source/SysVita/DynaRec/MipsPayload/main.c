#include <kermit.h>

#define REG32(addr) (*(volatile uint32_t *)(addr))
#define sync()      __asm__ __volatile__("sync" ::: "memory")

static void main_loop(void);

static void power_resume_handler(void *context) {
	kermit_interrupts_enable();
	// TODO: restore registers properly
	main_loop();
}

static void power_suspend_handler(enum kermit_virtual_interrupt id) {
	kermit_interrupts_disable();

	REG32(0xA8000008)++;

	kermit_suspend(power_resume_handler);
}

static void interrupt_1_handler(enum kermit_virtual_interrupt id) {
	REG32(0xA8000004)++;
}


static void check_interrupts(void) {
	uint32_t pending;
	uint16_t kermit_interrupts;

	pending = REG32(0xBC300030) & ~0x4002;
	REG32(0xBC300030) = pending;
	sync();

	kermit_interrupts = pending >> 16;
	if (kermit_interrupts)
		kermit_dispatch_virtual_interrupts(kermit_interrupts);
}

static void main_loop(void) {
	for(;;) {
		check_interrupts();
		REG32(0xA8000000)++;
	}
}

int main(void) {
	kermit_init();
	kermit_interrupts_enable();
	kermit_register_virtual_interrupt_handler(1, interrupt_1_handler);
	kermit_register_virtual_interrupt_handler(KERMIT_VIRTUAL_INTR_POWER_CH1, power_suspend_handler);
	main_loop();
	return 0;
}

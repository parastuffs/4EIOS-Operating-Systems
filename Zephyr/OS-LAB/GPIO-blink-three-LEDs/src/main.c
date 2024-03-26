/*
 * 2024, DLH
 * 
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/__assert.h>
#include <string.h>

/* size of stack area used by each thread */
#define STACKSIZE 1024

/* Scheduling priority used by each thread.
 * "A thread’s priority is an integer value, 
 *  and can be either negative or non-negative. 
 *  Numerically lower priorities takes precedence 
 *  over numerically higher values."
 * https://docs.zephyrproject.org/latest/kernel/services/threads/index.html#thread-priorities
 */
#define PRIORITY_A 7
#define PRIORITY_B 8
#define PRIORITY_C 9
#define PRIORITY_UART 7

/* Get a handle to the device from the device tree using an alias.
 * Aliases are defined in boards/riscv/longan_nano/longan_nano-common.dtsi
 */
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)

/* Get a handle to the device from the device tree using a label.
 * Those are defined in an overlay located at the project root ./app.overlay
 * or in the board folder ./boards/longan-nano.overlay
 */
#define A2_NODE DT_NODELABEL(a2)
#define A3_NODE DT_NODELABEL(a3)
#define A4_NODE DT_NODELABEL(a4)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS(LED1_NODE, okay)
#error "Unsupported board: led1 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS(LED2_NODE, okay)
#error "Unsupported board: led2 devicetree alias is not defined"
#endif

struct printk_data_t {
	void *fifo_reserved; /* 1st word reserved for use by fifo */
	uint32_t led;
	uint32_t cnt;
};

K_FIFO_DEFINE(printk_fifo);

struct gpio {
	/* Details of the gpio: port, label, active high or low, ... */
	struct gpio_dt_spec spec;
	/* Arbitrary ID used for debug print */
	uint8_t num;
};

static const struct gpio led0 = {
	.spec = GPIO_DT_SPEC_GET_OR(LED0_NODE, gpios, {0}),
	.num = 0,
};

static const struct gpio led1 = {
	.spec = GPIO_DT_SPEC_GET_OR(LED1_NODE, gpios, {0}),
	.num = 1,
};

static const struct gpio led2 = {
	.spec = GPIO_DT_SPEC_GET_OR(LED2_NODE, gpios, {0}),
	.num = 2,
};

static const struct gpio a2 = {
	.spec = GPIO_DT_SPEC_GET_OR(A2_NODE, gpios, {0}),
	.num = 0,
};

static const struct gpio a3 = {
	.spec = GPIO_DT_SPEC_GET_OR(A3_NODE, gpios, {0}),
	.num = 0,
};

static const struct gpio a4 = {
	.spec = GPIO_DT_SPEC_GET_OR(A4_NODE, gpios, {0}),
	.num = 0,
};

void blink(const struct gpio *led, const struct gpio *pin, uint32_t period_ms, uint32_t ontime_ms, uint32_t id)
{
	const struct gpio_dt_spec *spec = &led->spec;
	const struct gpio_dt_spec *spec_trig = &pin->spec;
	int cnt = 0;
	int ret;

	if (!device_is_ready(spec->port)) {
		printk("Error: %s device is not ready\n", spec->port->name);
		return;
	}

	ret = gpio_pin_configure_dt(spec, GPIO_OUTPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure pin %d (LED '%d')\n",
			ret, spec->pin, led->num);
		return;
	}
	
	gpio_pin_configure_dt(spec_trig, GPIO_OUTPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure pin %d (TRIG '%d')\n",
			ret, spec_trig->pin, pin->num);
		return;
	}

	while (1) {
		/* Set the value of the pin, 0 or 1 */
		gpio_pin_set(spec->port, spec->pin, cnt % 2);
		/*gpio_pin_set(spec_trig->port, spec_trig->pin, cnt % 2);*/
		/* Toggle the value of the pin */
		gpio_pin_toggle_dt(spec_trig);

		/* Prepare stuff to be written on the kernel output */
		struct printk_data_t tx_data = { .led = id, .cnt = cnt };
		size_t size = sizeof(struct printk_data_t);
		char *mem_ptr = k_malloc(size);
		__ASSERT_NO_MSG(mem_ptr != 0);
		memcpy(mem_ptr, &tx_data, size);
		k_fifo_put(&printk_fifo, mem_ptr);

		k_msleep(period_ms);
		cnt++;
	}
}

void blink0(void)
{
	blink(&led0, &a2, 200, 100,  0);
}

void blink1(void)
{
	blink(&led1, &a3, 500, 250, 1);
}

void blink2(void)
{
	blink(&led2, &a4, 1000, 500, 2);
}

void uart_out(void)
{
	while (1) {
		struct printk_data_t *rx_data = k_fifo_get(&printk_fifo,
							   K_FOREVER);
		printk("Toggled led%d; counter=%d\n",
		       rx_data->led, rx_data->cnt);
		k_free(rx_data);
	}
}

K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL,
		PRIORITY_A, 0, 0);
K_THREAD_DEFINE(blink1_id, STACKSIZE, blink1, NULL, NULL, NULL,
		PRIORITY_B, 0, 0);
K_THREAD_DEFINE(blink2_id, STACKSIZE, blink2, NULL, NULL, NULL,
		PRIORITY_C, 0, 0);
K_THREAD_DEFINE(uart_out_id, STACKSIZE, uart_out, NULL, NULL, NULL,
		PRIORITY_UART, 0, 0);

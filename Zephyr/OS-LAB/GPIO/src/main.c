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
#define PRIORITY_UART 12

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
#define A7_NODE DT_NODELABEL(a7) // Connect to CH3
#define A3_NODE DT_NODELABEL(a3) // Connect to CH2
#define A4_NODE DT_NODELABEL(a4) // Connect to CH1

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "Unsupported board: led0 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS(LED1_NODE, okay)
#error "Unsupported board: led1 devicetree alias is not defined"
#endif

#if !DT_NODE_HAS_STATUS(LED2_NODE, okay)
#error "Unsupported board: led2 devicetree alias is not defined"
#endif

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

static const struct gpio a7 = {
	.spec = GPIO_DT_SPEC_GET_OR(A7_NODE, gpios, {0}),
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

struct k_mutex my_mutex; 

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
	
	ret = gpio_pin_configure_dt(spec_trig, GPIO_OUTPUT);
	if (ret != 0) {
		printk("Error %d: failed to configure pin %d (TRIG '%d')\n",
			ret, spec_trig->pin, pin->num);
		return;
	}

	gpio_pin_set(spec->port, spec->pin, 0);
	gpio_pin_set(spec_trig->port, spec_trig->pin, 0);

	while (1) {
		uint32_t start_time = k_uptime_get_32();

		/* Set the value of the pin, 0 or 1 */
		gpio_pin_set(spec->port, spec->pin, 1);
		gpio_pin_set(spec_trig->port, spec_trig->pin, 1);
		k_mutex_lock(&my_mutex, K_FOREVER);
		k_msleep(ontime_ms);
		k_mutex_unlock(&my_mutex);
		gpio_pin_set(spec->port, spec->pin, 0);
		gpio_pin_set(spec_trig->port, spec_trig->pin, 0);

		/* Sleep cycles = period - time_elapsed */
		// TODO Test if it works with the proper `k_uptime_delta()` function
		// https://docs.zephyrproject.org/apidoc/latest/group__clock__apis.html#gad748b2fe83b36884dc087b4af367de80
		uint32_t elap_time = k_uptime_get_32() - start_time;
		uint32_t sleep_ms = period_ms - elap_time;
		printk("Thread: %d, elap_time=%d, sleep_ms=%d\n",id, elap_time, sleep_ms);
		if(sleep_ms > 0) {
			//printk("Going to sleep for %d \n", sleep_ms);
			k_msleep(sleep_ms);
		}

		/*k_msleep(period_ms);*/
		cnt++;
	}
}

void blink0(void)
{
	blink(&led0, &a7, 200, 25,  0);
}

void blink1(void)
{
	blink(&led1, &a3, 500, 100, 1);
}

void blink2(void)
{
	blink(&led2, &a4, 1000, 200, 2);
}

/* Define a mutex */
K_MUTEX_DEFINE(my_mutex);

K_THREAD_DEFINE(blink0_id, STACKSIZE, blink0, NULL, NULL, NULL,
		PRIORITY_A, 0, 0);
K_THREAD_DEFINE(blink1_id, STACKSIZE, blink1, NULL, NULL, NULL,
		PRIORITY_B, 0, 0);
K_THREAD_DEFINE(blink2_id, STACKSIZE, blink2, NULL, NULL, NULL,
		PRIORITY_C, 0, 0);
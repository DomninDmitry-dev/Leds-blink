#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>      /* For platform devices */
#include <linux/interrupt.h>            /* For IRQ */
#include <linux/gpio.h>                 /* For Legacy integer based GPIO */
#include <linux/of_gpio.h>              /* For of_gpio* functions */
#include <linux/of.h>                   /* For DT*/
#include <linux/gpio/consumer.h>
#include <asm/irq.h>

static struct gpio_desc *green, *red, *yellow, *btn;

// Callback простого таймера
static struct timer_list my_timer;
static u8 toggle = 0;
static unsigned int irq;

static irq_handler_t btn_pushed_irq(unsigned int irq, void *dev_id, struct pt_regs *regs)
{
	int state;
	/* read the button value and change the led state */
	state = gpiod_get_value(btn);
	gpiod_set_value(red, state);
	pr_info("btn interrupt: Interrupt! btn state is %d)\n", state);
	return (irq_handler_t)IRQ_HANDLED;
}

static void my_timer_callback(struct timer_list *t)
{
	//pr_info("Timer Handler called.\n");
	mod_timer(&my_timer, jiffies + HZ / 5); // Period 1s
	//pr_info("Jiffies: %ld\n", jiffies);
	switch(toggle)
	{
	case 0:
		gpiod_set_value(green, 0);
		gpiod_set_value(red, 0);
		gpiod_set_value(yellow, 0);
		break;
	case 1:
		gpiod_set_value(green, 1);
		gpiod_set_value(red, 0);
		gpiod_set_value(yellow, 0);
		break;
	case 2:
		gpiod_set_value(green, 0);
		gpiod_set_value(red, 1);
		gpiod_set_value(yellow, 0);
		break;
	case 3:
		gpiod_set_value(green, 0);
		gpiod_set_value(red, 0);
		gpiod_set_value(yellow, 1);
		break;
	}
	toggle++;
	if(toggle == 4) toggle = 0;
}

static int my_pdrv_probe(struct platform_device *pdev)
{
	int retval;
	struct device *dev = &pdev->dev;

	pr_info("GPIO: Starting module\n");

	// Использование обычного приближенного таймера
	timer_setup( &my_timer, my_timer_callback, 0);
	pr_info("Starting timer to fire in 200ms (%ld)\n", jiffies);
	retval = mod_timer(&my_timer, jiffies + HZ / 5); // Period 1s
	if(retval) pr_err("Error in mod_timer\n");

	green = gpiod_get_index(dev, "leds", 0, GPIOD_OUT_HIGH);
	red = gpiod_get_index(dev, "leds", 1, GPIOD_OUT_HIGH);
	yellow = gpiod_get_index(dev, "leds", 2, GPIOD_OUT_HIGH);
	//btn = gpiod_get(dev, "btn", GPIOD_IN);

	//irq = gpiod_to_irq(btn);
	irq = platform_get_irq(pdev, 0);
	retval = request_threaded_irq(irq, NULL,
								(irq_handler_t)btn_pushed_irq,
								IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
								"gpio-descriptor-sample", NULL);

	retval = gpiod_direction_output(green, 0);
	if(retval)
	{
		pr_err("GPIO: Error led-green <gpiod_direction_output> \n");
		return -1;
	}
	retval = gpiod_direction_output(red, 0);
	if(retval)
	{
		pr_err("GPIO: Error led-red <gpiod_direction_output> \n");
		return -1;
	}
	retval = gpiod_direction_output(yellow, 0);
	if(retval)
	{
		pr_err("GPIO: Error led-yellow <gpiod_direction_output> \n");
		return -1;
	}

    return 0;
}
static int my_pdrv_remove(struct platform_device *pdev)
{
	gpiod_set_value(green, 0);
	gpiod_put(green);
	gpiod_set_value(red, 0);
	gpiod_put(red);
	gpiod_set_value(yellow, 0);
	gpiod_put(yellow);
	gpiod_put(btn);
	free_irq(irq, NULL);
	del_timer(&my_timer);
    pr_info("GPIO: remove module\n");
    return 0;
}

static const struct of_device_id gpio_dt_ids[] = {
    { .compatible = "test,leds-blink" },
    { /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, gpio_dt_ids);

static struct platform_driver mypdrv = {
    .probe      = my_pdrv_probe,
    .remove     = my_pdrv_remove,
    .driver     = {
        .name     = "leds-blink",
        .of_match_table = of_match_ptr(gpio_dt_ids),
        .owner    = THIS_MODULE,
    },
};
module_platform_driver(mypdrv);
MODULE_AUTHOR("Domnin Dmitry");
MODULE_LICENSE("GPL");


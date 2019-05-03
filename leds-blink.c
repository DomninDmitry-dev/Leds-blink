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

enum leds_pin {
	PIN_P1,	/* Optional */
	PIN_P2,	/* Optional */
	PIN_P3,	/* Optional */
	PIN_P4,	/* Optional */
	PIN_P5,	/* Optional */
	PIN_P6,	/* Optional */
	PIN_MAX
};

struct leds_data {
	struct platform_device *pdev;
	struct gpio_desc *gpio[PIN_MAX];
	struct gpio_desc *btn;
	struct timer_list my_timer;
	u8 toggle;
	unsigned int irq;
	int gpiocnt;
	int tms;
};
/*----------------------------------------------------------------------------*/
static irqreturn_t btn_irq(int irq, void *data)
{
	struct leds_data *pdata = data;
	int state = gpiod_get_value(pdata->btn);
	dev_info(&pdata->pdev->dev, "Interrupt! btn state is %d)\n", state);

	return (irqreturn_t)IRQ_HANDLED;
}
/*----------------------------------------------------------------------------*/
static void my_timer_callback(struct timer_list *t)
{
	struct leds_data *pdata = container_of(t, struct leds_data, my_timer);

	mod_timer(&pdata->my_timer, jiffies + HZ / (1000 / pdata->tms));
	dev_info(&pdata->pdev->dev, "Jiffies: %ld\n", jiffies);
	//dev_info(&pdata->pdev->dev, "pdata->tms: %d\n", pdata->tms);

	/*switch(toggle)
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
	*/
}
/*----------------------------------------------------------------------------*/
static int my_pdrv_probe(struct platform_device *pdev)
{
	int ret, pins;
	struct leds_data *pdata;

	dev_info(&pdev->dev, "Starting module\n");

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Error mem <devm_kzalloc> \n");
		return -ENOMEM;
	}

	pdata->gpiocnt = gpiod_count(&pdev->dev, "gpiopins");
	if(pdata->gpiocnt < 0) {
		dev_err(&pdev->dev, "Error get count gpio! \n");
		goto fail;
	}
	dev_info(&pdev->dev, "Gpio count: %d\n", pdata->gpiocnt);

	/*
	 * Если используется devm_gpiod_get_index, то gpiod_put вызывается
	 * автоматически при выгрузке модуля
	 * */
	for (pins = 0; pins < pdata->gpiocnt; pins++) {
		pdata->gpio[pins] = devm_gpiod_get_index(&pdev->dev, "gpiopins", pins,
							  GPIOD_OUT_LOW);
		if (IS_ERR(pdata->gpio[pins])) {
			ret = PTR_ERR(pdata->gpio[pins]);
			dev_err(&pdev->dev, "Error get index gpio %d, err %d! \n", pins, ret);
			goto fail;
		}
		ret = gpiod_direction_output(pdata->gpio[pins], 0);
		if(ret < 0) {
			dev_err(&pdev->dev, "Error direction output gpio %d, err %d! \n", pins, ret);
			goto fail;
		}
	}

	pdata->btn = devm_gpiod_get(&pdev->dev, "btn", GPIOD_IN);
	if (IS_ERR(pdata->btn)) {
			ret = PTR_ERR(pdata->btn);
			dev_err(&pdev->dev, "Error get btn gpio, err %d! \n", ret);
			goto fail;
		}

	pdata->irq = platform_get_irq(pdev, 0);
	//pdata->irq = gpiod_to_irq(pdata->btn);
	dev_info(&pdev->dev, "irq = %d\n", pdata->irq);
	/*
	 * Если используется devm_request_threaded_irq, то devm_free_irq вызывается
	 * автоматически при выгрузке модуля
	 * */
	ret = devm_request_threaded_irq(&pdev->dev, pdata->irq, NULL,\
								btn_irq, \
								IRQF_TRIGGER_FALLING | IRQF_ONESHOT, \
								"leds-button", pdata);
	if(ret < 0) {
		dev_err(&pdev->dev, "Error IRQ - %d\n", ret);
		goto fail;
	}

	// Использование обычного приближенного таймера
	pdata->tms = 1000;
	timer_setup(&pdata->my_timer, my_timer_callback, 0);
	dev_info(&pdev->dev, "Starting timer to fire in %dms (%ld)\n", pdata->tms, jiffies);
	ret = mod_timer(&pdata->my_timer, jiffies + HZ / (1000 / pdata->tms));
	if(ret) {
		dev_err(&pdev->dev, "Error init timer - %d\n", ret);
		goto fail;
	}

	pdata->pdev = pdev;
	platform_set_drvdata(pdev, pdata);

	dev_info(&pdev->dev, "Probe started\n");
    return 0;

fail:
	devm_kfree(&pdev->dev, pdata);
	dev_err(&pdev->dev, "Probe fail!\n");
	return -1;
}
/*----------------------------------------------------------------------------*/
static int my_pdrv_remove(struct platform_device *pdev)
{
	struct leds_data *pdata = platform_get_drvdata(pdev);
	del_timer(&pdata->my_timer);
	devm_kfree(&pdev->dev, pdata);
	dev_info(&pdev->dev, "Removing module\n");
    return 0;
}
/*----------------------------------------------------------------------------*/
static const struct of_device_id gpio_dt_ids[] = {
    { .compatible = "orangepi,leds-blink" },
    { /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, gpio_dt_ids);
/*----------------------------------------------------------------------------*/
static struct platform_driver leds_driver = {
    .probe      = my_pdrv_probe,
    .remove     = my_pdrv_remove,
    .driver     = {
        .name     = "leds-blink",
        .of_match_table = of_match_ptr(gpio_dt_ids),
        .owner    = THIS_MODULE,
    },
};
module_platform_driver(leds_driver);
/*----------------------------------------------------------------------------*/
MODULE_DESCRIPTION("Leds blink driver");
MODULE_AUTHOR("Domnin Dmitry");
MODULE_LICENSE("GPL");
/*----------------------------------------------------------------------------*/

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
#include <linux/random.h>
#include <linux/device.h>

enum leds_pin {
	PIN_P1,	/* Optional */
	PIN_P2,	/* Optional */
	PIN_P3,	/* Optional */
	PIN_P4,	/* Optional */
	PIN_P5,	/* Optional */
	PIN_P6,	/* Optional */
	PIN_MAX
};
/*----------------------------------------------------------------------------*/
struct leds_data {
	struct platform_device *pdev;
	struct gpio_desc *gpio[PIN_MAX];
	struct gpio_desc *btn;
	struct timer_list my_timer;
	unsigned int irq;
	int gpiocnt;
	unsigned int tms;
	u32 ledMode;
	u8 tmp;
	u8 flag;
	struct device_attribute dev_attr_tms;
	struct device_attribute dev_attr_ledmode;
};
/*----------------------------------------------------------------------------*/
static ssize_t tms_show(struct device *child, struct device_attribute *attr, char *buf)
{
	struct leds_data *pdata = container_of(attr, struct leds_data, dev_attr_tms);

	return sprintf(buf, "%d\n", pdata->tms);
}
/*----------------------------------------------------------------------------*/
static ssize_t tms_store(struct device *child, struct device_attribute *attr, const char *buf, size_t len)
{
	struct leds_data *pdata = container_of(attr, struct leds_data, dev_attr_tms);

    sscanf(buf, "%d", &pdata->tms);
    dev_info(&pdata->pdev->dev, "sysfs_notify store %s: %d\n", pdata->dev_attr_tms.attr.name, pdata->tms);

    return len;
}
/*----------------------------------------------------------------------------*/
static ssize_t ledmode_show(struct device *child, struct device_attribute *attr, char *buf)
{
	struct leds_data *pdata = container_of(attr, struct leds_data, dev_attr_ledmode);

	return sprintf(buf, "%d\n", pdata->ledMode);
}
/*----------------------------------------------------------------------------*/
static ssize_t ledmode_store(struct device *child, struct device_attribute *attr, const char *buf, size_t len)
{
	struct leds_data *pdata = container_of(attr, struct leds_data, dev_attr_ledmode);
	u32 mode;

	pdata->flag = 0;
	pdata->tmp = 0;
    sscanf(buf, "%d", &mode);
    dev_info(&pdata->pdev->dev, "sysfs_notify store %s: %d\n", pdata->dev_attr_tms.attr.name, mode);

    if(mode > 3) return -1;
    else pdata->ledMode = mode;
    return len;
}
/*----------------------------------------------------------------------------*/
static irqreturn_t btn_irq(int irq, void *data)
{
	struct leds_data *pdata = data;
	int state = gpiod_get_value(pdata->btn);
	u8 cnt;
	dev_info(&pdata->pdev->dev, "Interrupt! btn state is %d)\n", state);
	if(++pdata->ledMode == 4) pdata->ledMode = 0;
	for(cnt=0; cnt<pdata->gpiocnt; cnt++)
		gpiod_set_value(pdata->gpio[cnt], 0);

	return (irqreturn_t)IRQ_HANDLED;
}
/*----------------------------------------------------------------------------*/
static void my_timer_callback(struct timer_list *t)
{
	struct leds_data *pdata = container_of(t, struct leds_data, my_timer);
	unsigned int timej;
	u8 cnt;

	timej = (pdata->tms >= 1000) ? (HZ * (pdata->tms / 1000)) : (HZ / (1000 / pdata->tms));
	mod_timer(&pdata->my_timer, jiffies + timej);
	//dev_info(&pdata->pdev->dev, "Jiffies: %ld\n", jiffies);
	pr_debug("pr_debug: Jiffies: %ld\n", jiffies);

	switch(pdata->ledMode)
	{
	case 0:
		for(cnt=0; cnt<pdata->gpiocnt; cnt++)
			gpiod_set_value(pdata->gpio[cnt], 1);
		break;
	case 1:
		if(pdata->tmp == 0) pdata->tmp = 1;
		for(cnt=0; cnt<pdata->gpiocnt; cnt++){
			if((pdata->tmp >> cnt) & 1)
				gpiod_set_value(pdata->gpio[cnt], 1);
			else gpiod_set_value(pdata->gpio[cnt], 0);
		}
		pdata->tmp <<= 1;
		for(cnt=0; cnt<pdata->gpiocnt; cnt++)
			if((pdata->tmp >> cnt) & 1) break;
		if(cnt == pdata->gpiocnt) pdata->tmp = 1;
		break;
	case 2:
		if(pdata->tmp == 0) pdata->tmp = 1;
		for(cnt=0; cnt<pdata->gpiocnt; cnt++){
			if((pdata->tmp >> cnt) & 1)
				gpiod_set_value(pdata->gpio[cnt], 1);
			else gpiod_set_value(pdata->gpio[cnt], 0);
		}
		if(!pdata->flag){
			pdata->tmp <<= 1;
			for(cnt=0; cnt<pdata->gpiocnt; cnt++)
				if((pdata->tmp >> cnt) & 1) break;
			if(cnt+1 == pdata->gpiocnt) pdata->flag = 1;
		}
		else {
			pdata->tmp >>= 1;
			if(pdata->tmp & 1) pdata->flag = 0;
		}
		break;
	case 3:
		for(cnt=0; cnt<pdata->gpiocnt; cnt++){
			if((pdata->tmp >> cnt) & 1)
				gpiod_set_value(pdata->gpio[cnt], 1);
			else gpiod_set_value(pdata->gpio[cnt], 0);
		}
		get_random_bytes(&pdata->tmp, 1);
		break;
	}
}
/*----------------------------------------------------------------------------*/
static int my_pdrv_probe(struct platform_device *pdev)
{
	int ret, pins;
	struct leds_data *pdata;
	unsigned int timej;

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
	pdata->tms = 0;
	if(of_find_property(pdev->dev.of_node, "timer_ms", NULL)) {
		of_property_read_u32(pdev->dev.of_node, "timer_ms", &pdata->tms);
		if(pdata->tms) {
			timer_setup(&pdata->my_timer, my_timer_callback, 0);
			dev_info(&pdev->dev, "Starting timer to fire in %dms (%ld)\n", pdata->tms, jiffies);
			timej = (pdata->tms >= 1000) ? (HZ * (pdata->tms / 1000)) : (HZ / (1000 / pdata->tms));
			ret = mod_timer(&pdata->my_timer, jiffies + timej);
			if(ret) {
				dev_err(&pdev->dev, "Error init timer - %d\n", ret);
				goto fail;
			}
		}
		else dev_info(&pdev->dev, "Timer value NULL");
	}
	else {
		dev_info(&pdev->dev, "Timer not init!");
	}

	// Считываем режим работы светодиодов
	pdata->ledMode = 0;
	if(of_find_property(pdev->dev.of_node, "mode", NULL)) {
		of_property_read_u32(pdev->dev.of_node, "mode", &pdata->ledMode);
	}

	pdata->dev_attr_tms.attr.name = "timer_ms";
	pdata->dev_attr_tms.attr.mode = S_IRUGO | S_IWUSR;
	pdata->dev_attr_tms.show = &tms_show;
	pdata->dev_attr_tms.store = &tms_store;
	if(device_create_file(&pdev->dev, &pdata->dev_attr_tms) != 0){
		dev_err(&pdev->dev, "Error create attribute file %s!", pdata->dev_attr_tms.attr.name);
		goto fail;
	}
	pdata->dev_attr_ledmode.attr.name = "ledmode";
	pdata->dev_attr_ledmode.attr.mode = S_IRUGO | S_IWUSR;
	pdata->dev_attr_ledmode.show = &ledmode_show;
	pdata->dev_attr_ledmode.store = &ledmode_store;
	if(device_create_file(&pdev->dev, &pdata->dev_attr_ledmode) != 0){
		device_remove_file(&pdev->dev, &pdata->dev_attr_tms);
		dev_err(&pdev->dev, "Error create attribute file %s!", pdata->dev_attr_ledmode.attr.name);
		goto fail;
	}

	pdata->flag = 0;
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
	u8 cnt;
	if(pdata->tms) del_timer(&pdata->my_timer);
	device_remove_file(&pdev->dev, &pdata->dev_attr_tms);
	device_remove_file(&pdev->dev, &pdata->dev_attr_ledmode);
	devm_kfree(&pdev->dev, pdata);
	for(cnt=0; cnt<pdata->gpiocnt; cnt++)
		gpiod_set_value(pdata->gpio[cnt], 0);
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

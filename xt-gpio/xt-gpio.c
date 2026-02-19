#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/serio.h>

#define DRIVER_NAME "xt-gpio"

#define XT_NUM_BITS     9

#define XT_START_BIT    0
#define XT_DATA_BIT0    1
#define XT_DATA_BIT1    2
#define XT_DATA_BIT2    3
#define XT_DATA_BIT3    4
#define XT_DATA_BIT4    5
#define XT_DATA_BIT5    6
#define XT_DATA_BIT6    7
#define XT_DATA_BIT7    8

/*
 * XT keyboard timing (similar to PS/2 but may vary)
 * Clock frequency: approximately 10-30 kHz
 */
#define XT_CLK_FREQ_MIN_HZ      10000   /* 10 kHz - conservative */
#define XT_CLK_FREQ_MAX_HZ      30000   /* 30 kHz - XT can be faster */

#define XT_CLK_MIN_INTERVAL_US  ((1000 * 1000) / XT_CLK_FREQ_MAX_HZ)  /* ~33 µs */
#define XT_CLK_MAX_INTERVAL_US  ((1000 * 1000) / XT_CLK_FREQ_MIN_HZ)  /* 100 µs */

/* Allow some margin for interrupt latency */
#define XT_IRQ_MIN_INTERVAL_US  (XT_CLK_MIN_INTERVAL_US - 20)   /* ~13 µs */
#define XT_IRQ_MAX_INTERVAL_US  (XT_CLK_MAX_INTERVAL_US + 20)   /* ~120 µs */

/*
 * Minimum idle time between bytes.
 * If time since last interrupt exceeds this, we should be at START.
 */
#define XT_MIN_IDLE_BETWEEN_BYTES_US          200     /* Minimum idle gap between bytes */

struct xt_drv_data {
    struct device *dev;
    struct serio *serio;
    struct gpio_desc *gpiod_clock;
    struct gpio_desc *gpiod_data;
    struct gpio_desc *gpiod_reset;
    uint8_t byte;
    int cnt;
    int irq;

    ktime_t t_irq_now;
    ktime_t t_irq_last;
    bool t_irq_last_valid;
};

static irqreturn_t gpio_irq_handler(int irq, void *dev_id) {
    struct xt_drv_data *drv_data = dev_id;
    int data;
    s64 us_delta;

    uint8_t byte = drv_data->byte;
    int cnt = drv_data->cnt;

    drv_data->t_irq_now = ktime_get();
    if (drv_data->t_irq_last_valid) {
        us_delta = ktime_to_us(ktime_sub(drv_data->t_irq_now, drv_data->t_irq_last));
    } else {
        /* First interrupt after open/resync: skip timing checks. */
        us_delta = -1;
    }

    /*
     * Case 1: Interrupt came too soon after previous one
     * This indicates a spurious interrupt or noise
     */
    if (unlikely(us_delta >= 0 && us_delta < XT_IRQ_MIN_INTERVAL_US)) {
        dev_dbg(drv_data->dev, "RX: spurious interrupt (%lld us)\n", us_delta);
        goto end;  /* Ignore it */
    }
    
    /*
     * Case 2: Long gap since last interrupt
     * 
     * If we're NOT at state 0 (START), this means we missed interrupts
     * and lost sync. Reset to START state.
     * 
     * If we ARE at state 0, a long gap is EXPECTED - it's the idle time
     * between bytes.
     */
    if (us_delta > XT_IRQ_MAX_INTERVAL_US) {
        if (cnt != 0) {
            /* We were in the middle of a byte and missed interrupts! */
            dev_err(drv_data->dev, 
                    "RX: timeout at state %d, missed interrupt (%lld us)\n",
                    cnt, us_delta);
            cnt = 0;
            byte = 0;
        }
        /* 
         * If cnt == 0, the long gap is normal - we're at START of new byte
         * Continue processing normally
         */
    }

    /* Case 3: We're at START (cnt == 0) but gap is too short for valid START */
    if (cnt == 0 && us_delta >= 0 && us_delta < XT_MIN_IDLE_BETWEEN_BYTES_US) {
        dev_warn(drv_data->dev,
                 "RX: START bit came too soon (%lld us), ignoring\n", us_delta);
        goto end;  /* Ignore - can't be a valid START */
    }

    drv_data->t_irq_last = drv_data->t_irq_now;
    drv_data->t_irq_last_valid = true;

    /*
     * Use logical GPIO value as configured by DT flags.
     * For this driver we decode protocol line levels, so DT should use
     * GPIO_ACTIVE_HIGH for clock/data.
     */
    data = gpiod_get_value(drv_data->gpiod_data);
    if (data < 0) {
        dev_err(drv_data->dev, "RX: failed to read data GPIO\n");
        goto err;
    }

        switch (cnt) {
    case XT_START_BIT:
        if (data) {
            dev_dbg(drv_data->dev, "RX: START bit LOW detected\n");
        }
        byte = 0; // Clear byte for new data
        break;

    case XT_DATA_BIT0:
    case XT_DATA_BIT1:
    case XT_DATA_BIT2:
    case XT_DATA_BIT3:
    case XT_DATA_BIT4:
    case XT_DATA_BIT5:
    case XT_DATA_BIT6:
        if (data)
            byte |= (1 << (cnt - 1));
        break;

    case XT_DATA_BIT7:
        /* Sample the last data bit */
        if (data)
            byte |= (1 << 7);

        /*
         * We're done! The stop bit is NOT clocked.
         * After this falling edge, the keyboard will:
         * 1. Release DATA line → goes HIGH (idle)
         * 2. Stop generating clock
         * 
         * We could optionally sample DATA after a short delay
         * to verify it's HIGH (stop bit), but there's no
         * clock edge to trigger an interrupt.
         */

        /* Deliver the completed byte */
        serio_interrupt(drv_data->serio, byte, 0);
        dev_dbg(drv_data->dev, "RX: received byte 0x%02x\n", byte);

        /* Reset state machine for next byte */
        cnt = 0;
        byte = 0;
        goto end;

    default:
        dev_err(drv_data->dev, "RX: invalid state %d\n", cnt);
        goto err;
    }

    cnt++;
    goto end;

err:
    cnt = 0;
    byte = 0;

end:
    drv_data->cnt = cnt;
    drv_data->byte = byte;
    return IRQ_HANDLED;
}

static int xt_gpio_open(struct serio *serio) {
    struct xt_drv_data *drv_data = serio->port_data;

    drv_data->cnt = 0;
    drv_data->byte = 0;
    drv_data->t_irq_last_valid = false;

    // Send a reset signal to the keyboard by toggling the reset GPIO
    if (drv_data->gpiod_reset) {
        gpiod_set_value_cansleep(drv_data->gpiod_reset, 0); // HIGH
        msleep(800);
        gpiod_set_value_cansleep(drv_data->gpiod_reset, 1); // LOW
        msleep(200);
        gpiod_set_value_cansleep(drv_data->gpiod_reset, 0); // HIGH
    }

    enable_irq(drv_data->irq);
    return 0;
}

static void xt_gpio_close(struct serio *serio) {
    struct xt_drv_data *drv_data = serio->port_data;
    disable_irq(drv_data->irq);
    drv_data->cnt = 0;
    drv_data->byte = 0;
    drv_data->t_irq_last_valid = false;
}

static int xt_gpio_probe(struct platform_device *pdev) {
    struct xt_drv_data *drv_data;
    struct serio *serio;
    int ret;

    drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
    if (!drv_data) return -ENOMEM;

    serio = kzalloc(sizeof(*serio), GFP_KERNEL);
    if (!serio)
        return -ENOMEM;

    drv_data->dev = &pdev->dev;
    drv_data->serio = serio;
    drv_data->cnt = 0;
    drv_data->t_irq_last_valid = false;

    platform_set_drvdata(pdev, drv_data);

	enum gpiod_flags gflags = GPIOD_IN;

    // Request GPIO
    drv_data->gpiod_clock = devm_gpiod_get(&pdev->dev, "clock", gflags);
    if (IS_ERR(drv_data->gpiod_clock)) {
        pr_err("gpiod_get failed (clock)\n");
        ret = PTR_ERR(drv_data->gpiod_clock);
        goto err_free_serio;
    }

    drv_data->gpiod_data = devm_gpiod_get(&pdev->dev, "data", gflags);
    if (IS_ERR(drv_data->gpiod_data)) {
        pr_err("gpiod_get failed (data)\n");
        ret = PTR_ERR(drv_data->gpiod_data);
        goto err_free_serio;
    }

    if (gpiod_is_active_low(drv_data->gpiod_clock))
        dev_warn(&pdev->dev, "clock GPIO is ACTIVE_LOW; protocol bits are line levels, ACTIVE_HIGH is recommended\n");
    if (gpiod_is_active_low(drv_data->gpiod_data))
        dev_warn(&pdev->dev, "data GPIO is ACTIVE_LOW; protocol bits are line levels, ACTIVE_HIGH is recommended\n");

    drv_data->gpiod_reset = devm_gpiod_get_optional(&pdev->dev, "reset", GPIOD_OUT_LOW);
    if (IS_ERR(drv_data->gpiod_reset)) {
        pr_err("gpiod_get failed (reset)\n");
        ret = PTR_ERR(drv_data->gpiod_reset);
        goto err_free_serio;
    }

    // Setup Interrupt
    drv_data->irq = platform_get_irq(pdev, 0);
    if (drv_data->irq < 0) {
        pr_err("platform_get_irq failed\n");
        ret = drv_data->irq;
        goto err_free_serio;
    }
    ret = devm_request_irq(&pdev->dev, drv_data->irq, gpio_irq_handler,
        IRQF_NO_THREAD | IRQF_NO_AUTOEN, DRIVER_NAME, drv_data);
    if (ret)
        goto err_free_serio;

    // Setup serio
    serio->id.type = SERIO_XT;
    serio->open = xt_gpio_open;
    serio->close = xt_gpio_close;
    serio->port_data = drv_data;
    serio->dev.parent = &pdev->dev;
	strscpy(serio->name, dev_name(&pdev->dev), sizeof(serio->name));
	strscpy(serio->phys, dev_name(&pdev->dev), sizeof(serio->phys));
    serio_register_port(serio);

    dev_info(&pdev->dev, "Driver initialized with Blocking I/O\n");

    return 0;

err_free_serio:
    kfree(serio);
    return ret;
}

static void xt_gpio_remove(struct platform_device *pdev) {
    struct xt_drv_data *drv_data = platform_get_drvdata(pdev);
    serio_unregister_port(drv_data->serio);
    dev_info(&pdev->dev, "Driver removed\n");
}

static const struct of_device_id xt_gpio_match[] = {
    { .compatible = "xt-gpio" },
    { }
};
MODULE_DEVICE_TABLE(of, xt_gpio_match);

static struct platform_driver xt_gpio_plat_driver = {
    .probe = xt_gpio_probe,
    .remove = xt_gpio_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = xt_gpio_match,
    },
};

module_platform_driver(xt_gpio_plat_driver);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrei");
MODULE_DESCRIPTION("GPIO-backed XT keyboard serio transport");

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/mod_devicetable.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h>

static int major;
static int irq_number;


// Wait queue and flag for blocking logic
static DECLARE_WAIT_QUEUE_HEAD(button_wait_queue);
static bool data_available = false;

static struct class *btn_class = NULL;
static struct gpio_desc *gpiod_clock = NULL;
static struct gpio_desc *gpiod_data = NULL;
static struct gpio_desc *gpiod_reset = NULL;

static const char kbd_map[] = { 0, // key numbers start with 1, but we want to have 0 at index 0 for easier calculations
    '\0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    '\0', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    '\0', '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', '\0', '\0',
    '\0', ' ', '\0',  // alt, space, caps lock
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',   // F1-F10
    '\0', '\0', // num lock, scroll lock
    '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0',  // numpad keys
};

static uint16_t value = 0;
static int vbuf_index = 9;
static int mult_num[] = {
    0, 1<<0x0, 1<<0x1, 1<<0x2, 1<<0x3, 1<<0x4, 1<<0x5, 1<<0x6, 1<<0x7,
    0, 1<<0x8, 1<<0x9, 1<<0xa, 1<<0xb, 1<<0xc, 1<<0xd, 1<<0xe, 1<<0xf,
};
static irqreturn_t gpio_irq_handler(int irq, void *dev_id) {
    int v = gpiod_get_value(gpiod_data);
    value += v * mult_num[vbuf_index];
    vbuf_index = (vbuf_index + 1) % 18;
    data_available = (vbuf_index == 0);
    wake_up_interruptible(&button_wait_queue); // Wake up waiting processes
    return IRQ_HANDLED;
}

//#define SHOW_HEX 
static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    // Block here until data_available is true (button pressed/released)
    if (wait_event_interruptible(button_wait_queue, data_available))
        return -ERESTARTSYS;
    uint8_t code1 = value & 0xFF;
    uint8_t code2 = (value >> 8) & 0xFF;
    bool is_ctrl = (code1 == 0x9d || code2 == 0x9d);
    bool is_shift = (code1 == 0xaa || code2 == 0xaa);
    int code;
    if (code1 == 0x9d || code1 == 0xaa) {
        code = code2 - 0x80;
    } else if (code2 == 0x9d || code2 == 0xaa) {
        code = code1 - 0x80;
    } else if (code1 == (code2 & 0x7F)) {
        code = code1;
    } else {
        code = 0; // Invalid code, will be handled later
    }
    char key_value = '\0';
    if (code != 0 && code < sizeof(kbd_map)) {
        key_value = kbd_map[code]; // Get the key value from the map
    }
    if (is_ctrl && key_value >='a' && key_value <= 'z') {
        key_value = key_value - 'a' + 1; // Convert to control character (e.g., Ctrl+A -> 0x01)
    }
    if (is_shift && key_value >='a' && key_value <= 'z') {
        key_value = key_value - 'a' + 'A'; // Convert to uppercase (e.g., Shift+a -> A)
    }
    data_available = false; // Reset flag for the next interrupt
#ifdef SHOW_HEX
    char prbuf[0x100];
    if (code == 0) {
        snprintf(prbuf, sizeof prbuf, "%04X [code==0]\n", value);
    } else if (key_value >= 32 && key_value <= 126) { // Printable ASCII range
        snprintf(prbuf, sizeof prbuf, "%04X '%c'\n", value, key_value);
    } else {
        snprintf(prbuf, sizeof prbuf, "%04X code=%02x\n", value, code);
    }
    int copy_length = MIN(len, strlen(prbuf));
    if (copy_to_user(buf, prbuf, copy_length))
        return -EFAULT;
#else
    if (code == 0 || key_value == 0) {
        vbuf_index = 0;
        value = 0;
        return 1; // Return 1 for invalid key to maintain infinite stream
    }
    if (copy_to_user(buf, &key_value, 1))
        return -EFAULT;
#endif
    vbuf_index = 0;
    value = 0;

#ifdef SHOW_HEX
    if (copy_length > 0) {
        return copy_length;
    }
#endif
    return 1;     // do not return 0 to have infinite stream
}

static struct file_operations fops = { .owner = THIS_MODULE, .read = dev_read };

static int btn_probe(struct platform_device *pdev) {
    int ret;

    printk(KERN_INFO "Step 0. Enter btn_probe.\n");

    // 1. Register Character Device
    major = register_chrdev(0, "button_driver", &fops);
    if (major < 0) return major;
    printk(KERN_INFO "Step 1. Character device registered: major=%d.\n", major);


    // 2. Automatically create /dev/button_driver
    btn_class = class_create("button_class");
    if (IS_ERR(btn_class)) {
        pr_err("Failed to create class\n");
        unregister_chrdev(major, "button_driver");
        return PTR_ERR(btn_class);
    }
    printk(KERN_INFO "Step 2a. Node /dev/button_driver created.\n");

    struct device *btn_device = device_create(btn_class, NULL, MKDEV(major, 0), NULL, "button_driver");
    if (IS_ERR(btn_device)) {
        pr_err("Failed to create device\n");
        class_destroy(btn_class);
        unregister_chrdev(major, "button_driver");
        return PTR_ERR(btn_device);
    }
    printk(KERN_INFO "Step 2b. Device created.\n");

    // 3. Request GPIO
    gpiod_clock = gpiod_get(&pdev->dev, "clock", GPIOD_IN);
    if (IS_ERR(gpiod_clock)) {
        pr_err("gpiod_get failed (clock)\n");
        return PTR_ERR(gpiod_clock);
    }
    printk(KERN_INFO "Step 3a. Call to gpio_get for clock: ok.\n");

    gpiod_data = gpiod_get(&pdev->dev, "data", GPIOD_IN);
    if (IS_ERR(gpiod_data)) {
        pr_err("gpiod_get failed (data)\n");
        gpiod_put(gpiod_clock);
        return PTR_ERR(gpiod_data);
    }
    printk(KERN_INFO "Step 3b. Call to gpio_get for data: ok.\n");

    gpiod_reset = gpiod_get(&pdev->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(gpiod_reset)) {
        pr_err("gpiod_get failed (reset)\n");
        gpiod_put(gpiod_clock);
        gpiod_put(gpiod_data);
        return PTR_ERR(gpiod_reset);
    }
    printk(KERN_INFO "Step 3c. Call to gpio_get for reset: ok.\n");

    // 4. Send a reset signal
    gpiod_set_value(gpiod_reset, 0); // HIGH
    printk(KERN_INFO "Step 4. Sending reset signal: HIGH.\n");
    msleep(2000);
    gpiod_set_value(gpiod_reset, 1); // LOW
    printk(KERN_INFO "Step 4. Sending reset signal: LOW.\n");
    msleep(1000);
    gpiod_set_value(gpiod_reset, 0); // HIGH
    printk(KERN_INFO "Step 4. Sending reset signal: HIGH.\n");

    // 5. Setup Interrupt
    irq_number = gpiod_to_irq(gpiod_clock);
    if (irq_number < 0) {
        pr_err("gpiod_to_irq failed\n");
        gpiod_put(gpiod_data);
        gpiod_put(gpiod_clock);
        return irq_number;
    }
    ret = devm_request_irq(&pdev->dev, irq_number, gpio_irq_handler, 
                       IRQF_TRIGGER_FALLING, "btn_irq", NULL);
    printk(KERN_INFO "Step 5. Interrupt setup: ok.\n");

    dev_info(&pdev->dev, "Driver initialized with Blocking I/O\n");
    printk(KERN_INFO "Driver initialization done.\n");

    return ret;
}

static void btn_remove(struct platform_device *pdev) {
    free_irq(irq_number, NULL);
    gpiod_put(gpiod_data);
    gpiod_put(gpiod_clock);
    gpiod_put(gpiod_reset);
    device_destroy(btn_class, MKDEV(major, 0)); // Clean up device node
    class_destroy(btn_class);                   // Clean up class
    unregister_chrdev(major, "button_driver");
}

static const struct of_device_id btn_of_match[] = {
    { .compatible = "custom,button-driver" },
    { }
};
MODULE_DEVICE_TABLE(of, btn_of_match);

static struct platform_driver btn_plat_driver = {
    .probe = btn_probe,
    .remove = btn_remove,
    .driver = {
        .name = "button_driver",
        .of_match_table = btn_of_match,
    },
};

module_platform_driver(btn_plat_driver);
MODULE_LICENSE("GPL");

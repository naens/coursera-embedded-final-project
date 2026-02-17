#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mod_devicetable.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/spinlock.h>

#define VBUF_SIZE 0x100

static int major;
static int irq_number;

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

struct button_drv_data {
    dev_t dev;
    struct class *btn_class;
    struct gpio_desc *gpiod_clock;
    struct gpio_desc *gpiod_data;
    struct gpio_desc *gpiod_reset;
    struct cdev cdev;
    uint8_t vbuf[VBUF_SIZE];
    int vbuf_read_index;
    int vbuf_write_index;
    int vbuf_bit_index;
    bool data_available;
    bool is_shift;
    bool is_ctrl;
    wait_queue_head_t button_wait_queue;
    spinlock_t lock;
};

static const int mult_num[] = {
    0, 1<<0x0, 1<<0x1, 1<<0x2, 1<<0x3, 1<<0x4, 1<<0x5, 1<<0x6, 1<<0x7
};
static irqreturn_t gpio_irq_handler(int irq, void *dev_id) {
    struct button_drv_data *drv_data = dev_id;
    unsigned long flags;

    spin_lock_irqsave(&drv_data->lock, flags);
    int v = gpiod_get_value(drv_data->gpiod_data);
    drv_data->vbuf[drv_data->vbuf_write_index] += v * mult_num[drv_data->vbuf_bit_index];
    drv_data->vbuf_bit_index = (drv_data->vbuf_bit_index + 1) % 9;
    if (drv_data->vbuf_bit_index == 0) {
        int next_write_index = (drv_data->vbuf_write_index + 1) % VBUF_SIZE;
        if (drv_data->vbuf[next_write_index] != 0) {
            drv_data->vbuf[drv_data->vbuf_read_index] = 0;
            drv_data->vbuf_read_index = (drv_data->vbuf_read_index + 1) % VBUF_SIZE;
        }
        drv_data->vbuf_write_index = (drv_data->vbuf_write_index + 1) % VBUF_SIZE;
        drv_data->data_available = true;
        wake_up_interruptible(&drv_data->button_wait_queue); // Wake up waiting processes
    }
    spin_unlock_irqrestore(&drv_data->lock, flags);
    return IRQ_HANDLED;
}

static int dev_open(struct inode *inode, struct file *file) {
    struct button_drv_data *drv_data = container_of(inode->i_cdev, struct button_drv_data, cdev);
    if (!drv_data) return -ENODEV;
    file->private_data = drv_data;
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *off) {
    struct button_drv_data *drv_data = file->private_data;
    unsigned long flags;

    // Block here until data_available is true (button pressed/released)
    if (wait_event_interruptible(drv_data->button_wait_queue, drv_data->data_available))
        return -ERESTARTSYS;

    spin_lock_irqsave(&drv_data->lock, flags);
    uint8_t code = drv_data->vbuf[drv_data->vbuf_read_index];
    if (code == 0x2a) {
        drv_data->is_shift = true;
    } else if (code == 0xaa) {
        drv_data->is_shift = false;
    } else if (code == 0x1d) {
        drv_data->is_ctrl = true;
    } else if (code == 0x9d) {
        drv_data->is_ctrl = false;
    }
    drv_data->vbuf[drv_data->vbuf_read_index] = 0; // Clear the buffer after reading
    drv_data->vbuf_read_index = (drv_data->vbuf_read_index + 1) % VBUF_SIZE;
    drv_data->data_available = false; // Reset flag for the next interrupt
    spin_unlock_irqrestore(&drv_data->lock, flags);

    char prbuf[0x100];
    if (code < sizeof(kbd_map) && kbd_map[code] >= ' ' && kbd_map[code] <= '~') {
        char key_char = kbd_map[code];
        if (drv_data->is_shift) {
            if (key_char >= 'a' && key_char <= 'z') {
                key_char = key_char - 'a' + 'A';
            } else {
                // Handle shifted versions of non-alphabetic keys
                switch (key_char) {
                    case '1': key_char = '!'; break;
                    case '2': key_char = '@'; break;
                    case '3': key_char = '#'; break;
                    case '4': key_char = '$'; break;
                    case '5': key_char = '%'; break;
                    case '6': key_char = '^'; break;
                    case '7': key_char = '&'; break;
                    case '8': key_char = '*'; break;
                    case '9': key_char = '('; break;
                    case '0': key_char = ')'; break;
                    case '-': key_char = '_'; break;
                    case '=': key_char = '+'; break;
                    case '[': key_char = '{'; break;
                    case ']': key_char = '}'; break;
                    case '\\': key_char = '|'; break;
                    case ';': key_char = ':'; break;
                    case '\'': key_char = '"'; break;
                    case ',': key_char = '<'; break;
                    case '.': key_char = '>'; break;
                    case '/': key_char = '?'; break;
                }
            }
        } else if (drv_data->is_ctrl) {
            if (key_char >= 'a' && key_char <= 'z') {
                key_char = key_char - 'a' + 1; // Ctrl + letter gives control character
            }
        }
        if (key_char < ' ' || key_char > '~') {
            snprintf(prbuf, sizeof prbuf, "%02X (%02X)\n", code, key_char);
        } else {
            snprintf(prbuf, sizeof prbuf, "%02X (%c)\n", code, key_char);
        }
    } else {
        snprintf(prbuf, sizeof prbuf, "%02X\n", code);
    }

    int copy_length = MIN(len, strlen(prbuf));
    if (copy_to_user(buf, prbuf, copy_length))
        return -EFAULT;

    if (copy_length > 0) {
        return copy_length;
    }

    return 1;     // do not return 0 to have infinite stream
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
};

static int btn_probe(struct platform_device *pdev) {
    struct button_drv_data *drv_data;
    int ret;

    drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
    if (!drv_data) return -ENOMEM;

    spin_lock_init(&drv_data->lock);
    init_waitqueue_head(&drv_data->button_wait_queue);
    drv_data->data_available = false;

    platform_set_drvdata(pdev, drv_data);

    // 0. Allocate a major number for the device
    ret = alloc_chrdev_region(&drv_data->dev, 0, 1, "btndrv");
    if (ret) {
        pr_err("Failed to allocate chrdev region\n");
        return ret;
    }
    major = MAJOR(drv_data->dev);  // Set major for logging/device creation
    printk(KERN_INFO "Allocated chrdev region: major=%d, minor=0\n", major);

    // 1. Register Character Device
    cdev_init(&drv_data->cdev, &fops);
    drv_data->cdev.owner = THIS_MODULE;
    ret = cdev_add(&drv_data->cdev, MKDEV(major, 0), 1);
    if (ret) return ret;
    printk(KERN_INFO "Step 1. Character device registered: major=%d.\n", major);


    // 2. Automatically create /dev/btndrv
    drv_data->btn_class = class_create("button_class");
    if (IS_ERR(drv_data->btn_class)) {
        pr_err("Failed to create class\n");
        unregister_chrdev_region(drv_data->dev, 1);
        return PTR_ERR(drv_data->btn_class);
    }
    printk(KERN_INFO "Step 2a. Node /dev/btndrv created.\n");

    struct device *btn_device = device_create(drv_data->btn_class, NULL,
            MKDEV(major, 0), NULL, "btndrv");
    if (IS_ERR(btn_device)) {
        pr_err("Failed to create device\n");
        class_destroy(drv_data->btn_class);
        unregister_chrdev_region(drv_data->dev, 1);
        return PTR_ERR(btn_device);
    }
    printk(KERN_INFO "Step 2b. Device created.\n");

    // 3. Request GPIO
    drv_data->gpiod_clock = gpiod_get(&pdev->dev, "clock", GPIOD_IN);
    if (IS_ERR(drv_data->gpiod_clock)) {
        pr_err("gpiod_get failed (clock)\n");
        return PTR_ERR(drv_data->gpiod_clock);
    }
    printk(KERN_INFO "Step 3a. Call to gpio_get for clock: ok.\n");

    drv_data->gpiod_data = gpiod_get(&pdev->dev, "data", GPIOD_IN);
    if (IS_ERR(drv_data->gpiod_data)) {
        pr_err("gpiod_get failed (data)\n");
        gpiod_put(drv_data->gpiod_clock);
        return PTR_ERR(drv_data->gpiod_data);
    }
    printk(KERN_INFO "Step 3b. Call to gpio_get for data: ok.\n");

    drv_data->gpiod_reset = gpiod_get(&pdev->dev, "reset", GPIOD_OUT_HIGH);
    if (IS_ERR(drv_data->gpiod_reset)) {
        pr_err("gpiod_get failed (reset)\n");
        gpiod_put(drv_data->gpiod_clock);
        gpiod_put(drv_data->gpiod_data);
        return PTR_ERR(drv_data->gpiod_reset);
    }
    printk(KERN_INFO "Step 3c. Call to gpio_get for reset: ok.\n");

    // 4. Send a reset signal
    gpiod_set_value(drv_data->gpiod_reset, 0); // HIGH
    printk(KERN_INFO "Step 4. Sending reset signal: HIGH.\n");
    msleep(2000);
    gpiod_set_value(drv_data->gpiod_reset, 1); // LOW
    printk(KERN_INFO "Step 4. Sending reset signal: LOW.\n");
    msleep(1000);
    gpiod_set_value(drv_data->gpiod_reset, 0); // HIGH
    printk(KERN_INFO "Step 4. Sending reset signal: HIGH.\n");

    // 5. Setup Interrupt
    irq_number = gpiod_to_irq(drv_data->gpiod_clock);
    if (irq_number < 0) {
        pr_err("gpiod_to_irq failed\n");
        gpiod_put(drv_data->gpiod_data);
        gpiod_put(drv_data->gpiod_clock);
        return irq_number;
    }
    ret = devm_request_irq(&pdev->dev, irq_number, gpio_irq_handler, IRQF_TRIGGER_FALLING,
        "btn_irq", drv_data);
    if (ret) return ret;
    printk(KERN_INFO "Step 5. Interrupt setup: ok.\n");

    dev_info(&pdev->dev, "Driver initialized with Blocking I/O\n");
    printk(KERN_INFO "Driver initialization done.\n");

    return ret;
}

static void btn_remove(struct platform_device *pdev) {
    struct button_drv_data *drv_data = platform_get_drvdata(pdev);
    device_destroy(drv_data->btn_class, drv_data->dev); // Clean up device node
    class_destroy(drv_data->btn_class);                   // Clean up class
    cdev_del(&drv_data->cdev);
    unregister_chrdev_region(drv_data->dev, 1);
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

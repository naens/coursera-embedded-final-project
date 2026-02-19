# XT GPIO Keyboard Driver (Raspberry Pi + Buildroot)

This driver (`xt-gpio`) is a GPIO transport for an XT keyboard.
It registers a `serio` port of type `SERIO_XT`, and Linux `xtkbd` translates
those scancodes into standard input events.

## 1) Kernel Configuration

Enable these options in `make linux-menuconfig`:

- `CONFIG_MODULES` (if building `xt-gpio` as a module)
- `CONFIG_INPUT`
- `CONFIG_INPUT_EVDEV`
- `CONFIG_SERIO`
- `CONFIG_KEYBOARD_XTKBD`
- `CONFIG_GPIOLIB`
- `CONFIG_GPIO_BCM2835`
- `CONFIG_OF`

Useful for debugging:

- `CONFIG_SERIO_RAW`
- `CONFIG_DYNAMIC_DEBUG`

## 2) Buildroot Configuration

Enable the external package:

- `BR2_PACKAGE_XT_GPIO=y`

This package:

- builds `xt-gpio.ko`
- builds `xt-gpio.dtbo`
- installs overlay to `output/images/rpi-firmware/overlays/xt-gpio.dtbo`

## 3) Device Tree Overlay

Current overlay file: `xt-gpio.dts`

Default GPIOs:

- `clock-gpios = <&gpio 17 (GPIO_ACTIVE_HIGH | GPIO_OPEN_DRAIN)>;`
- `data-gpios = <&gpio 4 (GPIO_ACTIVE_HIGH | GPIO_OPEN_DRAIN)>;`
- `reset-gpios = <&gpio 27 GPIO_ACTIVE_LOW>;` (optional in driver)
- IRQ on clock falling edge

Enable overlay in firmware `config.txt`:

```text
dtoverlay=xt-gpio
```

## 4) Module Loading

At boot, load both:

- `xtkbd`
- `xt-gpio`

Example modules-load file entry:

```text
xtkbd
xt-gpio
```

`xtkbd` is required because `xt-gpio` only transports raw XT bytes via serio.
`xtkbd` converts them to `/dev/input/event*` key events.

## 5) Electrical Notes (Important)

XT keyboard lines are 5V/open-collector style. Raspberry Pi GPIO is 3.3V-only.

- Use proper level shifting / protection.
- Use pull-ups suitable for Pi-side logic.
- Do not connect 5V keyboard lines directly to Pi GPIO.

## 6) Verify on Target

After boot:

```sh
dmesg | grep -Ei "xt-gpio|xtkbd|serio"
ls -l /sys/bus/serio/devices
grep -A6 -B2 "XT Keyboard" /proc/bus/input/devices
```

Optional live test:

```sh
evtest /dev/input/eventX
```

IRQ activity check:

```sh
grep -i xt-gpio /proc/interrupts
```

## 7) Troubleshooting

### No key events

- Check `xtkbd` is loaded and bound to your `serioX`.
- Confirm `xt-gpio` probed successfully in `dmesg`.
- Confirm IRQ count increases when pressing keys.

### `RX: start bit is not asserted`

Usually polarity mismatch or stale overlay on target.

- Ensure deployed DT overlay uses `GPIO_ACTIVE_HIGH` for `clock-gpios` and `data-gpios`.
- Ensure driver reads logical GPIO value directly (`gpiod_get_value`), without manual inversion.
- Reinstall updated `xt-gpio.dtbo` and reboot.

### `serio->open` appears not called

`xt_gpio_open()` is called only when a serio client opens the port.
For this setup, that client is `xtkbd`. If `xtkbd` is not loaded/bound, IRQ is not enabled
(driver requests IRQ with `IRQF_NO_AUTOEN`).

## 8) Current Behavior Notes

- Reset GPIO is optional.
- On open, driver currently applies a long reset sequence (2s/1s delays). If this is too slow,
  tune `xt_gpio_open()` in `xt-gpio.c`.
- Driver timing filters may need tuning depending on keyboard and wiring quality.

#include "device.h"

static struct device *device_table[DEVICE_MAX];
static struct driver *driver_table[DRIVER_MAX];

static uint32_t device_count;
static uint32_t driver_count;

static int string_equal(const char *a, const char *b)
{
    uint32_t i = 0;

    if (a == (void *)0 || b == (void *)0)
        return 0;

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i])
            return 0;
        i++;
    }

    return a[i] == b[i];
}

static int find_device(struct device *dev)
{
    uint32_t i;

    for (i = 0; i < device_count; i++) {
        if (device_table[i] == dev)
            return (int)i;
    }

    return -1;
}

static int find_driver(struct driver *drv)
{
    uint32_t i;

    for (i = 0; i < driver_count; i++) {
        if (driver_table[i] == drv)
            return (int)i;
    }

    return -1;
}

static int driver_matches(struct device *dev, struct driver *drv)
{
    if (dev == (void *)0 || drv == (void *)0)
        return 0;

    if (dev->compatible == (void *)0 ||
        drv->compatible == (void *)0)
        return 0;

    return string_equal(dev->compatible, drv->compatible);
}

int device_register(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    if (find_device(dev) >= 0)
        return -2;

    if (device_count >= DEVICE_MAX)
        return -3;

    dev->driver = (void *)0;
    dev->driver_data = (void *)0;
    dev->state = DEVICE_REGISTERED;

    device_table[device_count++] = dev;

    return 0;
}

int device_unregister(struct device *dev)
{
    int index;
    uint32_t i;

    if (dev == (void *)0)
        return -1;

    index = find_device(dev);

    if (index < 0)
        return -2;

    if (dev->state == DEVICE_BOUND)
        return -3;

    for (i = (uint32_t)index; i + 1 < device_count; i++)
        device_table[i] = device_table[i + 1];

    device_table[device_count - 1] = (void *)0;
    device_count--;

    dev->state = DEVICE_UNBOUND;

    return 0;
}

int driver_register(struct driver *drv)
{
    if (drv == (void *)0)
        return -1;

    if (find_driver(drv) >= 0)
        return -2;

    if (driver_count >= DRIVER_MAX)
        return -3;

    drv->registered = 1;
    driver_table[driver_count++] = drv;

    return 0;
}

int driver_unregister(struct driver *drv)
{
    int index;
    uint32_t i;

    if (drv == (void *)0)
        return -1;

    index = find_driver(drv);

    if (index < 0)
        return -2;

    for (i = 0; i < device_count; i++) {
        if (device_table[i]->driver == drv)
            return -3;
    }

    for (i = (uint32_t)index; i + 1 < driver_count; i++)
        driver_table[i] = driver_table[i + 1];

    driver_table[driver_count - 1] = (void *)0;
    driver_count--;

    drv->registered = 0;

    return 0;
}

int device_bind(struct device *dev, struct driver *drv)
{
    int result;

    if (dev == (void *)0 || drv == (void *)0)
        return -1;

    if (find_device(dev) < 0)
        return -2;

    if (find_driver(drv) < 0)
        return -3;

    if (dev->state != DEVICE_REGISTERED &&
        dev->state != DEVICE_UNBOUND)
        return -4;

    if (!driver_matches(dev, drv))
        return -5;

    if (drv->probe == (void *)0)
        return -6;

    result = drv->probe(dev);

    if (result != 0)
        return result;

    dev->driver = drv;
    dev->state = DEVICE_BOUND;

    return 0;
}

int device_unbind(struct device *dev)
{
    int result;

    if (dev == (void *)0)
        return -1;

    if (dev->state != DEVICE_BOUND ||
        dev->driver == (void *)0)
        return -2;

    if (dev->driver->remove != (void *)0) {
        result = dev->driver->remove(dev);
        if (result != 0)
            return result;
    }

    dev->driver = (void *)0;
    dev->driver_data = (void *)0;
    dev->state = DEVICE_UNBOUND;

    return 0;
}

int device_bind_all(void)
{
    uint32_t i;
    uint32_t bound = 0;
    uint32_t j;

    for (i = 0; i < device_count; i++) {
        struct device *dev = device_table[i];

        if (dev->state != DEVICE_REGISTERED &&
            dev->state != DEVICE_UNBOUND)
            continue;

        for (j = 0; j < driver_count; j++) {
            if (!driver_table[j]->registered)
                continue;

            if (!driver_matches(dev, driver_table[j]))
                continue;

            if (device_bind(dev, driver_table[j]) == 0) {
                bound++;
                break;
            }
        }
    }

    return (int)bound;
}

/*
 * v0.9.0 standalone architecture test.
 *
 * This is intentionally a synthetic peripheral.
 * v0.9.x will replace it with DTB-created devices and
 * then a real MMIO peripheral driver.
 */

static volatile uint32_t demo_probe_count;
static volatile uint32_t demo_remove_count;

static int demo_probe(struct device *dev)
{
    demo_probe_count++;

    dev->driver_data = (void *)0x12345678UL;

    return 0;
}

static int demo_remove(struct device *dev)
{
    demo_remove_count++;

    dev->driver_data = (void *)0;

    return 0;
}

int device_model_test(void)
{
    static struct device demo_device = {
        .name = "demo-pl011",
        .compatible = "arm,pl011",
        .base = 0x09000000UL,
        .size = 0x1000UL,
        .irq = 33,
        .driver_data = (void *)0,
        .driver = (void *)0,
        .state = DEVICE_UNREGISTERED
    };

    static struct driver demo_driver = {
        .name = "demo-pl011-driver",
        .compatible = "arm,pl011",
        .probe = demo_probe,
        .remove = demo_remove,
        .registered = 0
    };

    int result;
    int bind_result;
    int unbind_result;

    if (device_count != 0 || driver_count != 0)
        return -10;

    demo_probe_count = 0;
    demo_remove_count = 0;

    /*
     * Device registration.
     */
    result = device_register((void *)0);
    if (result != -1)
        return -11;

    result = device_register(&demo_device);
    if (result != 0)
        return -12;

    if (demo_device.state != DEVICE_REGISTERED)
        return -13;

    if (device_register(&demo_device) != -2)
        return -14;

    /*
     * Driver registration.
     */
    if (driver_register((void *)0) != -1)
        return -15;

    result = driver_register(&demo_driver);
    if (result != 0)
        return -16;

    if (driver_register(&demo_driver) != -2)
        return -17;

    /*
     * Automatic compatible matching.
     */
    bind_result = device_bind_all();

    if (bind_result != 1)
        return -18;

    if (demo_device.driver != &demo_driver)
        return -19;

    if (demo_device.state != DEVICE_BOUND)
        return -20;

    if (demo_probe_count != 1)
        return -21;

    if ((uintptr_t)demo_device.driver_data !=
        0x12345678UL)
        return -22;

    /*
     * Bound device cannot be unregistered.
     */
    if (device_unregister(&demo_device) != -3)
        return -23;

    /*
     * Unbind.
     */
    unbind_result = device_unbind(&demo_device);

    if (unbind_result != 0)
        return -24;

    if (demo_device.state != DEVICE_UNBOUND)
        return -25;

    if (demo_device.driver != (void *)0)
        return -26;

    if (demo_remove_count != 1)
        return -27;

    /*
     * Driver can now be unregistered.
     */
    if (driver_unregister(&demo_driver) != 0)
        return -28;

    /*
     * Device can now be unregistered.
     */
    if (device_unregister(&demo_device) != 0)
        return -29;

    if (device_count != 0 || driver_count != 0)
        return -30;

    return 0;
}

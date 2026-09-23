#include "device.h"
#include "fdt.h"

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

int device_add_resource(
    struct device *dev,
    enum resource_type type,
    uintptr_t start,
    uintptr_t end,
    uint32_t flags)
{
    struct resource *res;

    if (dev == (void *)0)
        return -1;

    if (type == RESOURCE_NONE)
        return -2;

    if (start > end)
        return -3;

    if (dev->resource_count >= RESOURCE_MAX_PER_DEVICE)
        return -4;

    res = &dev->resources[dev->resource_count];

    res->type = type;
    res->start = start;
    res->end = end;
    res->flags = flags;

    dev->resource_count++;

    return 0;
}

struct resource *device_get_resource(
    struct device *dev,
    enum resource_type type,
    uint32_t index)
{
    uint32_t i;
    uint32_t match = 0;

    if (dev == (void *)0)
        return (void *)0;

    for (i = 0; i < dev->resource_count; i++) {
        if (dev->resources[i].type != type)
            continue;

        if (match == index)
            return &dev->resources[i];

        match++;
    }

    return (void *)0;
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

    dev->state = DEVICE_UNREGISTERED;

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

static struct device dtb_pl011_device;

struct device *device_find_compatible(const char *compatible)
{
    uint32_t i;

    if (compatible == (void *)0)
        return (void *)0;

    for (i = 0; i < device_count; i++) {
        struct device *dev = device_table[i];

        if (dev == (void *)0)
            continue;

        if (string_equal(dev->compatible, compatible))
            return dev;
    }

    return (void *)0;
}

int device_discover_from_fdt_reg(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const char *name,
    const char *compatible)
{
    struct fdt_reg reg;
    int result;

    if (dtb == 0 ||
        info == (void *)0 ||
        name == (void *)0 ||
        compatible == (void *)0)
        return -1;

    result = fdt_find_compatible_reg(
        dtb,
        info,
        compatible,
        &reg);

    if (result != 0)
        return -2;

    dtb_pl011_device.name = name;
    dtb_pl011_device.compatible = compatible;
    dtb_pl011_device.resource_count = 0;
    dtb_pl011_device.driver_data = (void *)0;
    dtb_pl011_device.driver = (void *)0;
    dtb_pl011_device.state = DEVICE_UNREGISTERED;

    result = device_add_resource(
        &dtb_pl011_device,
        RESOURCE_MEM,
        reg.base,
        reg.base + reg.size - 1,
        0);

    if (result != 0)
        return -3;

    return device_register(&dtb_pl011_device);
}

static volatile uint32_t demo_probe_count;
static volatile uint32_t demo_remove_count;

static int demo_probe(struct device *dev)
{
    struct resource *mem;
    struct resource *irq;

    demo_probe_count++;

    mem = device_get_resource(dev, RESOURCE_MEM, 0);
    irq = device_get_resource(dev, RESOURCE_IRQ, 0);

    if (mem == (void *)0 || irq == (void *)0)
        return -40;

    if (mem->start != 0x09000000UL)
        return -41;

    if (mem->end != 0x09000fffUL)
        return -42;

    if (irq->start != 33 || irq->end != 33)
        return -43;

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
        .resource_count = 0,
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

    demo_device.resource_count = 0;

    if (device_add_resource(
            &demo_device,
            RESOURCE_MEM,
            0x09000000UL,
            0x09000fffUL,
            0) != 0)
        return -11;

    if (device_add_resource(
            &demo_device,
            RESOURCE_IRQ,
            33,
            33,
            0) != 0)
        return -12;

    if (device_add_resource(
            &demo_device,
            RESOURCE_MEM,
            0x1000,
            0x0fff,
            0) != -3)
        return -13;

    if (device_get_resource(
            &demo_device,
            RESOURCE_MEM,
            0) == (void *)0)
        return -14;

    if (device_get_resource(
            &demo_device,
            RESOURCE_IRQ,
            0) == (void *)0)
        return -15;

    result = device_register((void *)0);

    if (result != -1)
        return -16;

    result = device_register(&demo_device);

    if (result != 0)
        return -17;

    if (demo_device.resource_count != 2)
        return -18;

    if (device_register(&demo_device) != -2)
        return -19;

    if (driver_register((void *)0) != -1)
        return -20;

    result = driver_register(&demo_driver);

    if (result != 0)
        return -21;

    if (driver_register(&demo_driver) != -2)
        return -22;

    bind_result = device_bind_all();

    if (bind_result != 1)
        return -23;

    if (demo_device.driver != &demo_driver)
        return -24;

    if (demo_device.state != DEVICE_BOUND)
        return -25;

    if (demo_probe_count != 1)
        return -26;

    if ((uintptr_t)demo_device.driver_data !=
        0x12345678UL)
        return -27;

    if (device_unregister(&demo_device) != -3)
        return -28;

    unbind_result = device_unbind(&demo_device);

    if (unbind_result != 0)
        return -29;

    if (demo_device.state != DEVICE_UNBOUND)
        return -30;

    if (demo_remove_count != 1)
        return -31;

    if (driver_unregister(&demo_driver) != 0)
        return -32;

    if (device_unregister(&demo_device) != 0)
        return -33;

    if (device_count != 0 || driver_count != 0)
        return -34;

    return 0;
}

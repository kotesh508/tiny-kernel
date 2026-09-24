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

static struct device dtb_device_pool[DEVICE_MAX];

static struct device *dtb_get_free_device(void)
{
    uint32_t i;

    for (i = 0; i < DEVICE_MAX; i++) {
        if (dtb_device_pool[i].state == DEVICE_UNREGISTERED)
            return &dtb_device_pool[i];
    }

    return (void *)0;
}


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


int device_discover_from_fdt(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const char *name,
    const char *compatible)
{
    struct fdt_reg reg;
    struct fdt_irq irq;
    struct device *dev;
    int result;

    if (dtb == 0 ||
        info == (void *)0 ||
        name == (void *)0 ||
        compatible == (void *)0)
        return -1;

    dev = dtb_get_free_device();

    if (dev == (void *)0)
        return -2;

    result = fdt_find_compatible_reg(
        dtb,
        info,
        compatible,
        &reg);

    if (result != 0)
        return -3;

    result = fdt_find_compatible_irq(
        dtb,
        info,
        compatible,
        &irq);

    if (result != 0)
        return -4;

    dev->name = name;
    dev->compatible = compatible;
    dev->resource_count = 0;
    dev->driver_data = (void *)0;
    dev->driver = (void *)0;
    dev->state = DEVICE_UNREGISTERED;

    result = device_add_resource(
        dev,
        RESOURCE_MEM,
        reg.base,
        reg.base + reg.size - 1,
        0);

    if (result != 0)
        return -5;

    result = device_add_resource(
        dev,
        RESOURCE_IRQ,
        (uintptr_t)irq.irq,
        (uintptr_t)irq.irq,
        irq.flags);

    if (result != 0)
        return -6;

    return device_register(dev);
}

int device_discover_from_fdt_list(
    uintptr_t dtb,
    const struct fdt_header_info *info,
    const struct fdt_device_desc *desc,
    uint32_t count)
{
    uint32_t i;
    int result;

    if (dtb == 0 ||
        info == (void *)0 ||
        desc == (void *)0 ||
        count == 0)
        return -1;

    if (count > DEVICE_MAX)
        return -2;

    for (i = 0; i < count; i++) {
        if (desc[i].name == (void *)0 ||
            desc[i].compatible == (void *)0)
            return -3;

        result = device_discover_from_fdt(
            dtb,
            info,
            desc[i].name,
            desc[i].compatible);

        if (result != 0)
            return -4;
    }

    return 0;
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

static int lifecycle_fail_probe(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    return -42;
}

static int lifecycle_remove(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    return 0;
}

static struct device lifecycle_fail_device = {
    .name = "lifecycle-fail-device",
    .compatible = "test,lifecycle-fail",
    .resource_count = 0,
    .driver_data = (void *)0,
    .driver = (void *)0,
    .state = DEVICE_UNREGISTERED
};

static struct driver lifecycle_fail_driver = {
    .name = "lifecycle-fail-driver",
    .compatible = "test,lifecycle-fail",
    .probe = lifecycle_fail_probe,
    .remove = lifecycle_remove,
    .registered = 0
};

static uint32_t lifecycle_success_probe_count;
static uint32_t lifecycle_success_remove_count;
static uint32_t lifecycle_success_state;

static int lifecycle_success_probe(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    lifecycle_success_probe_count++;
    dev->driver_data = &lifecycle_success_state;

    return 0;
}

static int lifecycle_success_remove(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    if (dev->driver_data != &lifecycle_success_state)
        return -2;

    lifecycle_success_remove_count++;

    /*
     * Intentionally do not clear driver_data here.
     *
     * device_unbind() must clear it after remove() succeeds.
     */
    return 0;
}

static struct device lifecycle_success_device = {
    .name = "lifecycle-success-device",
    .compatible = "test,lifecycle-success",
    .resource_count = 0,
    .driver_data = (void *)0,
    .driver = (void *)0,
    .state = DEVICE_UNREGISTERED
};

static struct driver lifecycle_success_driver = {
    .name = "lifecycle-success-driver",
    .compatible = "test,lifecycle-success",
    .probe = lifecycle_success_probe,
    .remove = lifecycle_success_remove,
    .registered = 0
};

int device_lifecycle_test(void)
{
    int result;

    /*
     * ------------------------------------------------------------
     * Phase 1: probe failure
     * ------------------------------------------------------------
     */

    lifecycle_fail_device.resource_count = 0;
    lifecycle_fail_device.driver_data = (void *)0;
    lifecycle_fail_device.driver = (void *)0;
    lifecycle_fail_device.state = DEVICE_UNREGISTERED;

    lifecycle_fail_driver.registered = 0;

    result = device_register(&lifecycle_fail_device);

    if (result != 0)
        return -1;

    if (lifecycle_fail_device.state != DEVICE_REGISTERED)
        return -2;

    result = driver_register(&lifecycle_fail_driver);

    if (result != 0)
        return -3;

    result = device_bind(
        &lifecycle_fail_device,
        &lifecycle_fail_driver);

    if (result != -42)
        return -4;

    if (lifecycle_fail_device.state != DEVICE_REGISTERED)
        return -5;

    if (lifecycle_fail_device.driver != (void *)0)
        return -6;

    if (lifecycle_fail_device.driver_data != (void *)0)
        return -7;

    result = device_unbind(&lifecycle_fail_device);

    if (result != -2)
        return -8;

    result = driver_unregister(&lifecycle_fail_driver);

    if (result != 0)
        return -9;

    result = device_unregister(&lifecycle_fail_device);

    if (result != 0)
        return -10;

    if (lifecycle_fail_device.state != DEVICE_UNREGISTERED)
        return -11;

    /*
     * ------------------------------------------------------------
     * Phase 2: successful bind -> remove -> unbind
     * ------------------------------------------------------------
     */

    lifecycle_success_probe_count = 0;
    lifecycle_success_remove_count = 0;
    lifecycle_success_state = 0x12345678UL;

    lifecycle_success_device.resource_count = 0;
    lifecycle_success_device.driver_data = (void *)0;
    lifecycle_success_device.driver = (void *)0;
    lifecycle_success_device.state = DEVICE_UNREGISTERED;

    lifecycle_success_driver.registered = 0;

    result = device_register(&lifecycle_success_device);

    if (result != 0)
        return -12;

    result = driver_register(&lifecycle_success_driver);

    if (result != 0)
        return -13;

    result = device_bind(
        &lifecycle_success_device,
        &lifecycle_success_driver);

    if (result != 0)
        return -14;

    if (lifecycle_success_device.state != DEVICE_BOUND)
        return -15;

    if (lifecycle_success_device.driver !=
        &lifecycle_success_driver)
        return -16;

    if (lifecycle_success_device.driver_data !=
        &lifecycle_success_state)
        return -17;

    if (lifecycle_success_probe_count != 1)
        return -18;

    /*
     * A bound device must not be unregisterable.
     */
    result = device_unregister(&lifecycle_success_device);

    if (result != -3)
        return -19;

    /*
     * A bound driver must not be unregisterable.
     */
    result = driver_unregister(&lifecycle_success_driver);

    if (result != -3)
        return -20;

    result = device_unbind(&lifecycle_success_device);

    if (result != 0)
        return -21;

    if (lifecycle_success_remove_count != 1)
        return -22;

    if (lifecycle_success_device.state != DEVICE_UNBOUND)
        return -23;

    if (lifecycle_success_device.driver != (void *)0)
        return -24;

    /*
     * Framework cleanup must happen after remove().
     */
    if (lifecycle_success_device.driver_data != (void *)0)
        return -25;

    /*
     * ------------------------------------------------------------
     * Phase 3: rebind after unbind
     * ------------------------------------------------------------
     */

    result = device_bind(
        &lifecycle_success_device,
        &lifecycle_success_driver);

    if (result != 0)
        return -26;

    if (lifecycle_success_device.state != DEVICE_BOUND)
        return -27;

    if (lifecycle_success_device.driver !=
        &lifecycle_success_driver)
        return -28;

    if (lifecycle_success_device.driver_data !=
        &lifecycle_success_state)
        return -29;

    if (lifecycle_success_probe_count != 2)
        return -30;

    result = device_unbind(&lifecycle_success_device);

    if (result != 0)
        return -31;

    if (lifecycle_success_remove_count != 2)
        return -32;

    if (lifecycle_success_device.state != DEVICE_UNBOUND)
        return -33;

    if (lifecycle_success_device.driver != (void *)0)
        return -34;

    if (lifecycle_success_device.driver_data != (void *)0)
        return -35;

    /*
     * ------------------------------------------------------------
     * Final cleanup
     * ------------------------------------------------------------
     */

    result = driver_unregister(&lifecycle_success_driver);

    if (result != 0)
        return -36;

    result = device_unregister(&lifecycle_success_device);

    if (result != 0)
        return -37;

    if (lifecycle_success_device.state != DEVICE_UNREGISTERED)
        return -38;

    return 0;
}

static int multi_pl031_probe(struct device *dev)
{
    struct resource *mem;
    struct resource *irq;

    if (dev == (void *)0)
        return -1;

    mem = device_get_resource(dev, RESOURCE_MEM, 0);
    irq = device_get_resource(dev, RESOURCE_IRQ, 0);

    if (mem == (void *)0 || irq == (void *)0)
        return -2;

    dev->driver_data = dev;

    return 0;
}

static int multi_pl031_remove(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    dev->driver_data = (void *)0;

    return 0;
}

static int multi_pl061_probe(struct device *dev)
{
    struct resource *mem;
    struct resource *irq;

    if (dev == (void *)0)
        return -1;

    mem = device_get_resource(dev, RESOURCE_MEM, 0);
    irq = device_get_resource(dev, RESOURCE_IRQ, 0);

    if (mem == (void *)0 || irq == (void *)0)
        return -2;

    dev->driver_data = dev;

    return 0;
}

static int multi_pl061_remove(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    dev->driver_data = (void *)0;

    return 0;
}

static struct driver multi_pl031_driver = {
    .name = "multi-pl031-driver",
    .compatible = "arm,pl031",
    .probe = multi_pl031_probe,
    .remove = multi_pl031_remove,
    .registered = 0
};

static struct driver multi_pl061_driver = {
    .name = "multi-pl061-driver",
    .compatible = "arm,pl061",
    .probe = multi_pl061_probe,
    .remove = multi_pl061_remove,
    .registered = 0
};

int device_multi_test(uintptr_t dtb,
                      const struct fdt_header_info *info)
{
    static const struct fdt_device_desc desc[] = {
        {
            .name = "pl031",
            .compatible = "arm,pl031"
        },
        {
            .name = "pl061",
            .compatible = "arm,pl061"
        }
    };

    struct device *pl011;
    struct device *pl031;
    struct device *pl061;
    int result;
    int bound;

    pl011 = device_find_compatible("arm,pl011");

    if (pl011 == (void *)0 ||
        pl011->state != DEVICE_BOUND)
        return -1;

    result = device_discover_from_fdt_list(
        dtb,
        info,
        desc,
        2);

    if (result != 0)
        return -2;

    pl031 = device_find_compatible("arm,pl031");
    pl061 = device_find_compatible("arm,pl061");

    if (pl031 == (void *)0 || pl061 == (void *)0)
        return -3;

    result = driver_register(&multi_pl031_driver);

    if (result != 0)
        return -4;

    result = driver_register(&multi_pl061_driver);

    if (result != 0)
        return -5;

    bound = device_bind_all();

    if (bound != 2)
        return -6;

    if (pl011->state != DEVICE_BOUND)
        return -7;

    if (pl031->state != DEVICE_BOUND)
        return -8;

    if (pl061->state != DEVICE_BOUND)
        return -9;

    if (pl031->driver != &multi_pl031_driver)
        return -10;

    if (pl061->driver != &multi_pl061_driver)
        return -11;

    if (pl031->driver_data == (void *)0)
        return -12;

    if (pl061->driver_data == (void *)0)
        return -13;

    result = device_unbind(pl031);

    if (result != 0)
        return -14;

    result = device_unbind(pl061);

    if (result != 0)
        return -15;

    result = driver_unregister(&multi_pl031_driver);

    if (result != 0)
        return -16;

    result = driver_unregister(&multi_pl061_driver);

    if (result != 0)
        return -17;

    result = device_unregister(pl031);

    if (result != 0)
        return -18;

    result = device_unregister(pl061);

    if (result != 0)
        return -19;

    return 0;
}

static int hardening_probe(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    return 0;
}

static int hardening_remove(struct device *dev)
{
    if (dev == (void *)0)
        return -1;

    return 0;
}

static struct device hardening_device = {
    .name = "hardening-device",
    .compatible = "test,hardening",
    .resource_count = 0,
    .driver_data = (void *)0,
    .driver = (void *)0,
    .state = DEVICE_UNREGISTERED
};

static struct driver hardening_driver = {
    .name = "hardening-driver",
    .compatible = "test,hardening",
    .probe = hardening_probe,
    .remove = hardening_remove,
    .registered = 0
};

static struct device hardening_wrong_device = {
    .name = "hardening-wrong-device",
    .compatible = "test,device",
    .resource_count = 0,
    .driver_data = (void *)0,
    .driver = (void *)0,
    .state = DEVICE_UNREGISTERED
};

static struct driver hardening_wrong_driver = {
    .name = "hardening-wrong-driver",
    .compatible = "test,driver",
    .probe = hardening_probe,
    .remove = hardening_remove,
    .registered = 0
};

static struct device hardening_fill_devices[DEVICE_MAX];
static struct device hardening_extra_device;

static struct driver hardening_fill_drivers[DRIVER_MAX];
static struct driver hardening_extra_driver;

int device_hardening_test(void)
{
    struct resource *res;
    int result;
    uint32_t i;
    uint32_t registered_count;

    /*
     * ------------------------------------------------------------
     * NULL argument checks
     * ------------------------------------------------------------
     */

    if (device_register((void *)0) != -1)
        return -1;

    if (device_unregister((void *)0) != -1)
        return -2;

    if (driver_register((void *)0) != -1)
        return -3;

    if (driver_unregister((void *)0) != -1)
        return -4;

    /*
     * ------------------------------------------------------------
     * Resource validation
     * ------------------------------------------------------------
     */

    hardening_device.resource_count = 0;
    hardening_device.driver_data = (void *)0;
    hardening_device.driver = (void *)0;
    hardening_device.state = DEVICE_UNREGISTERED;

    result = device_add_resource(
        &hardening_device,
        RESOURCE_MEM,
        0x2000,
        0x1fff,
        0);

    if (result != -3)
        return -5;

    for (i = 0; i < RESOURCE_MAX_PER_DEVICE; i++) {
        result = device_add_resource(
            &hardening_device,
            RESOURCE_MEM,
            (uintptr_t)(0x3000 + i * 0x100),
            (uintptr_t)(0x30ff + i * 0x100),
            0);

        if (result != 0)
            return -6;
    }

    res = device_get_resource(
        &hardening_device,
        RESOURCE_MEM,
        RESOURCE_MAX_PER_DEVICE - 1);

    if (res == (void *)0)
        return -7;

    result = device_add_resource(
        &hardening_device,
        RESOURCE_MEM,
        0x4000,
        0x40ff,
        0);

    if (result != -4)
        return -8;

    /*
     * ------------------------------------------------------------
     * Duplicate device
     * ------------------------------------------------------------
     */

    hardening_device.resource_count = 0;
    hardening_device.driver_data = (void *)0;
    hardening_device.driver = (void *)0;
    hardening_device.state = DEVICE_UNREGISTERED;

    result = device_register(&hardening_device);

    if (result != 0)
        return -9;

    if (device_register(&hardening_device) != -2)
        return -10;

    /*
     * ------------------------------------------------------------
     * Duplicate driver
     * ------------------------------------------------------------
     */

    hardening_driver.registered = 0;

    result = driver_register(&hardening_driver);

    if (result != 0)
        return -11;

    if (driver_register(&hardening_driver) != -2)
        return -12;

    /*
     * ------------------------------------------------------------
     * Wrong compatible
     * ------------------------------------------------------------
     */

    hardening_wrong_device.resource_count = 0;
    hardening_wrong_device.driver_data = (void *)0;
    hardening_wrong_device.driver = (void *)0;
    hardening_wrong_device.state = DEVICE_UNREGISTERED;

    hardening_wrong_driver.registered = 0;

    result = device_register(&hardening_wrong_device);

    if (result != 0)
        return -13;

    result = driver_register(&hardening_wrong_driver);

    if (result != 0)
        return -14;

    if (device_bind(
            &hardening_wrong_device,
            &hardening_wrong_driver) != -5)
        return -15;

    if (hardening_wrong_device.state != DEVICE_REGISTERED)
        return -16;

    /*
     * ------------------------------------------------------------
     * Double bind / unregister protection / double unbind
     * ------------------------------------------------------------
     */

    result = device_bind(
        &hardening_device,
        &hardening_driver);

    if (result != 0)
        return -17;

    if (hardening_device.state != DEVICE_BOUND)
        return -18;

    if (device_bind(
            &hardening_device,
            &hardening_driver) != -4)
        return -19;

    if (device_unregister(&hardening_device) != -3)
        return -20;

    if (driver_unregister(&hardening_driver) != -3)
        return -21;

    if (device_unbind(&hardening_device) != 0)
        return -22;

    if (hardening_device.state != DEVICE_UNBOUND)
        return -23;

    if (device_unbind(&hardening_device) != -2)
        return -24;

    if (hardening_device.driver != (void *)0)
        return -25;

    if (hardening_device.driver_data != (void *)0)
        return -26;

    /*
     * ------------------------------------------------------------
     * Cleanup wrong-compatible test
     * ------------------------------------------------------------
     */

    result = driver_unregister(&hardening_wrong_driver);

    if (result != 0)
        return -27;

    result = device_unregister(&hardening_wrong_device);

    if (result != 0)
        return -28;

    /*
     * ------------------------------------------------------------
     * Cleanup normal test
     * ------------------------------------------------------------
     */

    result = driver_unregister(&hardening_driver);

    if (result != 0)
        return -29;

    result = device_unregister(&hardening_device);

    if (result != 0)
        return -30;

    /*
     * ------------------------------------------------------------
     * Device table full
     * ------------------------------------------------------------
     */

    registered_count = 0;

    for (i = 0; i < DEVICE_MAX; i++) {
        hardening_fill_devices[i].name = "hardening-fill-device";
        hardening_fill_devices[i].compatible =
            "test,hardening-fill";
        hardening_fill_devices[i].resource_count = 0;
        hardening_fill_devices[i].driver_data = (void *)0;
        hardening_fill_devices[i].driver = (void *)0;
        hardening_fill_devices[i].state =
            DEVICE_UNREGISTERED;

        result = device_register(
            &hardening_fill_devices[i]);

        if (result != 0)
            break;

        registered_count++;
    }

    if (registered_count == DEVICE_MAX) {
        hardening_extra_device.name =
            "hardening-extra-device";
        hardening_extra_device.compatible =
            "test,hardening-fill";
        hardening_extra_device.resource_count = 0;
        hardening_extra_device.driver_data = (void *)0;
        hardening_extra_device.driver = (void *)0;
        hardening_extra_device.state =
            DEVICE_UNREGISTERED;

        if (device_register(
                &hardening_extra_device) != -3)
            return -31;
    } else {
        if (result != -3)
            return -32;
    }

    for (i = 0; i < registered_count; i++) {
        if (device_unregister(
                &hardening_fill_devices[i]) != 0)
            return -33;
    }

    /*
     * ------------------------------------------------------------
     * Driver table full
     * ------------------------------------------------------------
     */

    registered_count = 0;

    for (i = 0; i < DRIVER_MAX; i++) {
        hardening_fill_drivers[i].name =
            "hardening-fill-driver";
        hardening_fill_drivers[i].compatible =
            "test,hardening-fill-driver";
        hardening_fill_drivers[i].probe =
            hardening_probe;
        hardening_fill_drivers[i].remove =
            hardening_remove;
        hardening_fill_drivers[i].registered = 0;

        result = driver_register(
            &hardening_fill_drivers[i]);

        if (result != 0)
            break;

        registered_count++;
    }

    if (registered_count == DRIVER_MAX) {
        hardening_extra_driver.name =
            "hardening-extra-driver";
        hardening_extra_driver.compatible =
            "test,hardening-fill-driver";
        hardening_extra_driver.probe =
            hardening_probe;
        hardening_extra_driver.remove =
            hardening_remove;
        hardening_extra_driver.registered = 0;

        if (driver_register(
                &hardening_extra_driver) != -3)
            return -34;
    } else {
        if (result != -3)
            return -35;
    }

    for (i = 0; i < registered_count; i++) {
        if (driver_unregister(
                &hardening_fill_drivers[i]) != 0)
            return -36;
    }

    return 0;
}

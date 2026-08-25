#include <stddef.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

// TODO: Modify this so it is a device based configuration rather than hardcoded...
#define TUI_EMMC_NODE "/soc@03000000/sdmmc@04022000"

static struct platform_device *tui_emmc_pdev;

static int __init tui_emmc_init(void) {
    struct platform_device *existing;
    struct platform_device *parent_pdev = NULL;
    struct device_node *parent_np;
    struct device_node *np;
    const char *status = NULL;
    int ret;

    np = of_find_node_by_path(TUI_EMMC_NODE);
    if (!np) {
        pr_err("tui_emmc: DT node %s not found\n", TUI_EMMC_NODE);
        return -ENODEV;
    }

    of_property_read_string(np, "status", &status);
    pr_info("tui_emmc: found %s (status=%s)\n", TUI_EMMC_NODE, status ? status : "<missing>");

    if (of_device_is_available(np)) {
        pr_err("tui_emmc: refusing available DT node\n");
        ret = -EBUSY;
        goto out_put_node;
    }

    existing = of_find_device_by_node(np);
    if (existing) {
        put_device(&existing->dev);
        pr_err("tui_emmc: platform device already exists\n");
        ret = -EEXIST;
        goto out_put_node;
    }

    parent_np = of_get_parent(np);
    if (parent_np) {
        parent_pdev = of_find_device_by_node(parent_np);
        of_node_put(parent_np);
    }

    tui_emmc_pdev = of_device_alloc(np, NULL, parent_pdev ? &parent_pdev->dev : NULL);
    if (!tui_emmc_pdev) {
        ret = -ENOMEM;
        goto out_put_parent;
    }

    tui_emmc_pdev->dev.bus = &platform_bus_type;
    of_dma_configure(&tui_emmc_pdev->dev, tui_emmc_pdev->dev.of_node);

    ret = device_add(&tui_emmc_pdev->dev);
    if (ret) {
        pr_err("tui_emmc: device_add failed: %d\n", ret);
        platform_device_put(tui_emmc_pdev);
        tui_emmc_pdev = NULL;
        goto out_put_parent;
    }

    pr_info(
        "tui_emmc: registered %s; bound driver=%s\n", dev_name(&tui_emmc_pdev->dev),
        tui_emmc_pdev->dev.driver ? tui_emmc_pdev->dev.driver->name : "<none>"
    );

out_put_parent:
    if (parent_pdev) put_device(&parent_pdev->dev);
out_put_node:
    of_node_put(np);
    return ret;
}

static void __exit tui_emmc_exit(void) {
    if (!tui_emmc_pdev) return;

    pr_info("tui_emmc: unregistering %s\n", dev_name(&tui_emmc_pdev->dev));
    of_device_unregister(tui_emmc_pdev);
    tui_emmc_pdev = NULL;
}

module_init(tui_emmc_init);
module_exit(tui_emmc_exit);

MODULE_DESCRIPTION("Late probe for TrimUI internal eMMC (sdc2)");
MODULE_AUTHOR("MustardOS");
MODULE_LICENSE("GPL");

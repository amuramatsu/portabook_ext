#include <linux/module.h>
#include <linux/kernel.h>

MODULE_DESCRIPTION("Portabook extra Module");
MODULE_AUTHOR("MURAMATSU Atsushi <amura@tomato.sakura.ne.jp>");
MODULE_LICENSE("GPL");

extern int portabook_backlight_init(void);
extern void portabook_backlight_cleanup(void);
extern int portabook_battery_init(void);
extern void portabook_battery_cleanup(void);

static int
portabook_ext_init_module(void)
{
    int error = 0;

    printk("portabook_ext is loaded!\n");
    error = portabook_battery_init();
    if (error)
	return error;
    error = portabook_backlight_init();
    if (error)
	return error;
    return 0;
}

static void portabook_ext_cleanup_module(void)
{
    portabook_battery_cleanup();
    portabook_backlight_cleanup();
}

module_init(portabook_ext_init_module);
module_exit(portabook_ext_cleanup_module);

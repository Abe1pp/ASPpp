#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0xd1614c70, "module_layout" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xd1c90ddd, "class_destroy" },
	{ 0x20a08b59, "device_destroy" },
	{ 0x733eb4b9, "cdev_del" },
	{ 0x3dc6510e, "device_create" },
	{ 0x25f39b1f, "cdev_add" },
	{ 0xa202a8e5, "kmalloc_order_trace" },
	{ 0x1d1ce098, "cdev_init" },
	{ 0xba7e2088, "__class_create" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x37a0cba, "kfree" },
	{ 0xd2b09ce5, "__kmalloc" },
	{ 0xdcb764ad, "memset" },
	{ 0x84bc974b, "__arch_copy_from_user" },
	{ 0xb35dea8f, "__arch_copy_to_user" },
	{ 0xcf2a6966, "up" },
	{ 0x25170ad2, "down_interruptible" },
	{ 0x1fdc7df2, "_mcount" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "8E91258D231C15EB2B00935");

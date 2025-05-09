load(":moto_product.bzl","mmi_product_name")
load(":kleaf-scripts/moto/moto_modules_define.bzl","moto_dist_module")
#moto common modules
load("//motorola/kernel/modules:drivers/misc/utag/moto_module_build.bzl", utag_driver = "define_moto_module_build")

moto_common_module_list = [ "utags", ]

def moto_common_modules():
    utag_driver()

def define_moto_modules():
    moto_common_modules()

    moto_dist_module(
        name = "mmi_modules_dist",
        module_list = moto_common_module_list,
    )

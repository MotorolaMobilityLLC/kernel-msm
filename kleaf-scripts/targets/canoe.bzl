load("//build/kernel/kleaf:kernel.bzl", "kernel_abi", "kernel_module_group")
load(":configs/canoe_consolidate.bzl", "canoe_consolidate_config")
load(":configs/canoe_perf.bzl", "canoe_perf_config")
load(":configs/canoe_tuivm.bzl", "canoe_tuivm_config")
load(":configs/canoe_tuivm_debug.bzl", "canoe_tuivm_debug_config")
load(":configs/ext_config/moto_perf_config.bzl", "moto_perf_config")
load(":configs/ext_config/moto_consolidate_config.bzl", "moto_consolidate_config")
load(":kleaf-scripts/android_build.bzl", "define_typical_android_build")
load(":kleaf-scripts/image_opts.bzl", "boot_image_opts")
load(":kleaf-scripts/vm_build.bzl", "define_typical_vm_build")
load(":target_variants.bzl", "la_variants")

target_name = "canoe"

def define_canoe():
    for variant in la_variants:
        board_kernel_cmdline_extras = []
        board_bootconfig_extras = []
        kernel_vendor_cmdline_extras = ["bootconfig"]

        if variant == "consolidate":
            board_bootconfig_extras += ["androidboot.serialconsole=1"]
            board_kernel_cmdline_extras += [
                # do not sort
                "console=ttyMSM0,115200n8",
                "qcom_geni_serial.con_enabled=1",
                "earlycon",
                "ufshcd_core.uic_cmd_timeout=2000",
            ]
            kernel_vendor_cmdline_extras += [
                # do not sort
                "console=ttyMSM0,115200n8",
                "qcom_geni_serial.con_enabled=1",
                "earlycon",
            ]

            consolidate_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a9c000",
                kernel_vendor_cmdline_extras = kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

        else:
            board_kernel_cmdline_extras += ["nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"]
            kernel_vendor_cmdline_extras += ["nosoftlockup console=ttynull qcom_geni_serial.con_enabled=0"]
            board_bootconfig_extras += ["androidboot.serialconsole=0"]

            perf_build_img_opts = boot_image_opts(
                earlycon_addr = "qcom_geni,0x00a9c000",
                kernel_vendor_cmdline_extras = kernel_vendor_cmdline_extras,
                board_kernel_cmdline_extras = board_kernel_cmdline_extras,
                board_bootconfig_extras = board_bootconfig_extras,
            )

    build_perf_config = canoe_perf_config | moto_perf_config
    build_consolidate_config = canoe_consolidate_config | moto_consolidate_config

    define_typical_android_build(
        name = "canoe",
        consolidate_config = build_consolidate_config,
        perf_config = build_perf_config,
        consolidate_build_img_opts = consolidate_build_img_opts,
        perf_build_img_opts = perf_build_img_opts,
        consolidate_kwargs = {
            "config_path": "configs/canoe_consolidate.bzl",
        },
        perf_kwargs = {
            "config_path": "configs/canoe_perf.bzl",
        },
    )

    kernel_abi(
        name = "canoe_perf_abi",
        kernel_build = "//common:kernel_aarch64",
        kernel_modules = [
            ":canoe_perf_all_modules",
        ],
    )

def define_canoe_tuivm():
    define_typical_vm_build(
        name = "canoe-tuivm",
        config = canoe_tuivm_config,
        debug_config = canoe_tuivm_debug_config,
        dtb_target = "canoe-tuivm",
        debug_kwargs = {
            "config_path": "configs/canoe_tuivm_debug.bzl",
        },
        config_kwargs = {
            "config_path": "configs/canoe_tuivm.bzl",
        },
    )

def define_canoe_oemvm():
    define_typical_vm_build(
        name = "canoe-oemvm",
        config = canoe_tuivm_config,
        debug_config = canoe_tuivm_debug_config,
        dtb_target = "canoe-oemvm",
        # Do not set config_path because it conflicts with canoe-tuivm
    )

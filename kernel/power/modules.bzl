def register_modules(registry):
    registry.register(
        name = "kernel/power/user_sysfs_private",
        out = "user_sysfs_private.ko",
        config = "CONFIG_SUSPEND_DEBUG",
        srcs = [
            # do not sort
            "kernel/power/user_sysfs_private.c",
            "kernel/power/user_sysfs_private.h",
        ],
        deps = [
            # do not sort
            "drivers/pinctrl/qcom/pinctrl-msm",
            "drivers/regulator/debug-regulator",
            "drivers/soc/qcom/qcom_stats",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

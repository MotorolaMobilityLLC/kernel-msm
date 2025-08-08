def register_modules(registry):
    registry.register(
        name = "drivers/virt/gunyah/gh_ctrl",
        out = "gh_ctrl.ko",
        config = "CONFIG_GH_CTRL",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_ctrl.c",
            "drivers/virt/gunyah/hcall_ctrl.h",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_dbl",
        out = "gh_dbl.ko",
        config = "CONFIG_GH_DBL",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_dbl.c",
            "drivers/virt/gunyah/hcall_dbl.h",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_irq_lend",
        out = "gh_irq_lend.ko",
        config = "CONFIG_GH_IRQ_LEND",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_irq_lend.c",
            "drivers/virt/gunyah/gh_rm_drv_private.h",
        ],
        deps = [
            # do not sort
            "drivers/virt/gunyah/gunyah_loader",
            "drivers/soc/qcom/mdt_loader",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_mem_notifier",
        out = "gh_mem_notifier.ko",
        config = "CONFIG_GH_MEM_NOTIFIER",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_mem_notifier.c",
        ],
        deps = [
            # do not sort
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_rm_heap_manager",
        out = "gh_rm_heap_manager.ko",
        config = "CONFIG_GH_RM_HEAP_MANAGER",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_rm_heap_manager.c",
            "include/linux/gunyah/gh_rm_heap_manager.h",
            "drivers/virt/gunyah/gh_rm_drv_private.h",
            "include/linux/mem-prot.h",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/mem-prot",
            "drivers/virt/gunyah/gh_rm_drv",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_msgq",
        out = "gh_msgq.ko",
        config = "CONFIG_GH_MSGQ",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_msgq.c",
            "drivers/virt/gunyah/hcall_msgq.h",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_panic_notifier",
        out = "gh_panic_notifier.ko",
        config = "CONFIG_GH_PANIC_NOTIFIER",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_panic_notifier.c",
        ],
        deps = [
            # do not sort
            "drivers/virt/gunyah/gunyah_loader",
            "drivers/virt/gunyah/gh_vcpu_mgr",
            "drivers/soc/qcom/mdt_loader",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_rm_booster",
        out = "gh_rm_booster.ko",
        config = "CONFIG_GH_RM_BOOSTER",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_rm_booster.c",
            "drivers/virt/gunyah/gh_rm_booster.h",
            "drivers/virt/gunyah/gh_rm_drv_private.h",
        ],
        deps = [
            # do not sort
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_rm_drv",
        out = "gh_rm_drv.ko",
        config = "CONFIG_GH_RM_DRV",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_rm_core.c",
            "drivers/virt/gunyah/gh_rm_drv_private.h",
            "drivers/virt/gunyah/gh_rm_iface.c",
        ],
        deps = [
            # do not sort
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_virt_wdt",
        out = "gh_virt_wdt.ko",
        config = "CONFIG_GH_VIRT_WATCHDOG",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_virt_wdt.c",
        ],
        deps = [
            # do not sort
            "drivers/soc/qcom/qcom_wdt_core",
            "drivers/soc/qcom/minidump",
            "drivers/soc/qcom/smem",
            "drivers/soc/qcom/debug_symbol",
            "drivers/dma-buf/heaps/qcom_dma_heaps",
            "drivers/iommu/msm_dma_iommu_mapping",
            "drivers/soc/qcom/mem_buf/mem_buf_dev",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_guest_pops",
        out = "gh_guest_pops.ko",
        config = "CONFIG_GH_GUEST_POPS",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_guest_pops.c",
        ],
        deps = [
            # do not sort
            "drivers/virt/gunyah/gh_rm_drv",
        ],
    )
    registry.register(
        name = "drivers/virt/gunyah/gh_virtio_pci_edge",
        out = "gh_virtio_pci_edge.ko",
        config = "CONFIG_GH_VIRTIO_PCI_EDGE",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_virtio_pci_edge.c",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gunyah_loader",
        out = "gunyah_loader.ko",
        config = "CONFIG_GH_SECURE_VM_LOADER",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_main.c",
            "drivers/virt/gunyah/gh_private.h",
            "drivers/virt/gunyah/gh_proxy_sched.h",
            "drivers/virt/gunyah/gh_secure_vm_loader.h",
            "drivers/virt/gunyah/gh_secure_vm_virtio_backend.c",
            "drivers/virt/gunyah/gh_secure_vm_virtio_backend.h",
            "drivers/virt/gunyah/hcall_virtio.h",
        ],
        conditional_srcs = {
            "CONFIG_GH_SECURE_VM_LOADER": {
                True: [
                    # do not sort
                    "drivers/virt/gunyah/gh_secure_vm_loader.c",
                ],
            },
            "CONFIG_GH_PROXY_SCHED": {
                True: [
                    # do not sort
                    "drivers/virt/gunyah/gh_proxy_sched.c",
                    "drivers/virt/gunyah/gh_proxy_sched_trace.h",
                ],
            },
        },
        deps = [
            # do not sort
            "drivers/soc/qcom/mdt_loader",
            "drivers/soc/qcom/secure_buffer",
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gunyah_qcom",
        out = "gunyah_qcom.ko",
        config = "CONFIG_GUNYAH_QCOM_PLATFORM",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gunyah_qcom.c",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "drivers/virt/gunyah/gh_msgq",
            "drivers/virt/gunyah/gh_dbl",
            "arch/arm64/gunyah/gh_arm_drv",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gh_vcpu_mgr",
        out = "gh_vcpu_mgr.ko",
        config = "CONFIG_GH_VCPU_MGR",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gh_vcpu_mgr.c",
            "drivers/virt/gunyah/gh_vcpu_mgr.h",
            "drivers/virt/gunyah/gh_vcpu_mgr_trace.h",
        ],
        deps = [
            # do not sort
            "drivers/firmware/qcom/qcom-scm",
            "drivers/virt/gunyah/gh_rm_drv",
            "arch/arm64/gunyah/gh_arm_drv",
            "drivers/soc/qcom/socinfo",
        ],
    )

    registry.register(
        name = "drivers/virt/gunyah/gunyah_trace",
        out = "gunyah_trace.ko",
        config = "CONFIG_GUNYAH_TRACE",
        srcs = [
            # do not sort
            "drivers/virt/gunyah/gunyah_trace.c",
            "drivers/virt/gunyah/gunyah_trace.h",
            "drivers/virt/gunyah/hcall_trace.h",
        ],
        deps = [
            # do not sort
            "drivers/virt/gunyah/gunyah",
            "arch/arm64/gunyah/gunyah_hypercall",
        ],
    )

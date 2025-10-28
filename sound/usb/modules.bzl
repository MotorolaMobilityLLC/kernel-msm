def register_modules(registry):
    registry.register(
        name = "sound/usb/snd-usb-audio-qmi",
        out = "snd-usb-audio-qmi.ko",
        config = "CONFIG_SND_USB_AUDIO_QMI",
        srcs = [
            # do not sort
            "sound/usb/usb_audio_qmi_svc.c",
            "sound/usb/quirks.c",
            "sound/usb/quirks.h",
            "sound/usb/usb_audio_qmi_v01.c",
            "sound/usb/usb_audio_qmi_v01.h",
        ],
        deps = [
            # do not sort
            "drivers/usb/host/xhci-sideband",
            "drivers/soc/qcom/qmi_helpers",
            "kernel/trace/qcom_ipc_logging",
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

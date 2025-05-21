def register_modules(registry):
    registry.register(
        name = "drivers/usb/redriver/nb7vpq904m",
        out = "nb7vpq904m.ko",
        config = "CONFIG_USB_REDRIVER_NB7VPQ904M",
        srcs = [
            # do not sort
            "drivers/usb/redriver/nb7vpq904m.c",
        ],
        deps = [
            # do not sort
            "drivers/usb/redriver/redriver",
        ],
    )

    registry.register(
        name = "drivers/usb/redriver/redriver",
        out = "redriver.ko",
        config = "CONFIG_USB_REDRIVER",
        srcs = [
            # do not sort
            "drivers/usb/redriver/redriver.c",
        ],
    )

    registry.register(
        name = "drivers/usb/redriver/ps5169",
        out = "ps5169.ko",
        config = "CONFIG_USB_REDRIVER_PS5169",
        srcs = [
            # do not sort
            "drivers/usb/redriver/ps5169.c",
        ],
        deps = [
            # do not sort
            "drivers/usb/redriver/redriver",
        ],
    )

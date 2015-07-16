ifeq ($(CONFIG_OF),y)

dtb-$(CONFIG_ARCH_MSM8994) += huawei_lunch_box_va.dtb \
                              huawei_angler_v1.dtb \
                              huawei_angler_v2.dtb \
                              huawei_angler_v3.dtb \
                              huawei_angler_v4.dtb \
                              huawei_angler_vn1.dtb \
                              huawei_angler_row_v1.dtb \
                              huawei_angler_row_v3.dtb \
                              huawei_angler_row_vn1.dtb

DTB_NAMES := $(subst $\",,$(CONFIG_BUILD_ARM64_APPENDED_DTB_IMAGE_NAMES))
ifneq ($(DTB_NAMES),)
DTB_LIST := $(addsuffix .dtb,$(DTB_NAMES))
else
DTB_LIST := $(dtb-y)
endif

targets += dtbs
targets += $(addprefix ../, $(DTB_LIST))
endif

$(obj)/../%.dtb: $(src)/%.dts FORCE
	$(call if_changed_dep,dtc)

dtbs: $(addprefix $(obj)/../,$(DTB_LIST))

clean-files := *.dtb

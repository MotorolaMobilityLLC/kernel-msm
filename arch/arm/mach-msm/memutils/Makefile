
mach-mem-y		:= memcpy.o copy_from_user.o copy_to_user.o copy_page.o

ifeq ($(CONFIG_ARCH_MSM8974),y)
  mach-mem-y	+= memmove_8974.o
else
  mach-mem-y	+= memmove.o
endif
obj-$(CONFIG_HAS_MACH_MEMUTILS) += $(mach-mem-y)

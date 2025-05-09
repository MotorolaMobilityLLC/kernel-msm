load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")
load(":moto_product.bzl",
"mmi_product_name",
"build_target",
"build_variant"
)

def moto_ddk_module(
    name,
    srcs = None,
    header_deps = [],
    hdrs = None,
    ko_deps = [],
    includes = None,
    conditional_srcs = {},
    conditional_defines = None,
    linux_includes = None,
    out = None,
    local_defines = None,
    copts = None):

    if out == None:
        out = name + ".ko"

    ddk_module(
        name = "{}_{}_{}".format(build_target, build_variant, name),
        srcs = srcs,
        out = "{}".format(out),
        local_defines = local_defines,
        includes = includes,
        conditional_srcs = conditional_srcs,
        linux_includes = linux_includes,
        hdrs = hdrs,
        copts = copts,
        deps = ["//soc-repo:all_headers"] + header_deps + ko_deps,
        kernel_build = "//soc-repo:{}_{}_base_kernel".format(build_target, build_variant),
        visibility = ["//visibility:public"]
    )


def moto_dist_module(
    name = [],
    module_list = []):

    data = []

    for module in module_list:
        data.append(":{}_{}_{}".format(build_target, build_variant, module))

    copy_to_dist_dir(
        name = "{}_{}_{}_dist".format(build_target, build_variant, name),
        data = data,
        dist_dir = "out/target/product/{}/dlkm/lib/modules/".format(build_target),
        flat = True,
        wipe_dist_dir = False,
        allow_duplicate_filenames = False,
        mode_overrides = {"**/*": "644"},
    )

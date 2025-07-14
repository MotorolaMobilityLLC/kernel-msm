load("//build/kernel/kleaf:kernel.bzl", "ddk_module")
load("//build/bazel_common_rules/dist:dist.bzl", "copy_to_dist_dir")
load(":moto_product.bzl", "build_target", "build_variant")

def split_lable(lable):
    parts = lable.split(":")
    return parts[0], parts[1]


def moto_ddk_module(
    name,
    srcs = None,
    hdrs = None,
    deps = None,
    deps_ext = None,
    includes = None,
    conditional_srcs = {},
    linux_includes = None,
    out = None,
    local_defines = None,
    copts = None):

    module_name = "{}_{}_{}".format(build_target, build_variant, name)
    module_dist_data = ":{}".format(module_name)

    if out == None:
        out = name + ".ko"

    module_all_deps = []

    if deps:
        for dep in deps:
            module_all_deps.append(dep)
    if deps_ext:
        for dep in deps_ext:
            dep_package, dep_target_name = split_lable(dep)
            if dep_package == "//soc-repo":
                module_all_deps.append("{}:{}_{}/{}".format(dep_package, build_target, build_variant, dep_target_name))
            else:
                module_all_deps.append("{}:{}_{}_{}".format(dep_package, build_target, build_variant, dep_target_name))
    module_all_deps.append("//soc-repo:{}_{}_config".format(build_target, build_variant,))

    ddk_module(
        name = module_name,
        srcs = srcs,
        out = "{}".format(out),
        local_defines = local_defines,
        includes = includes,
        conditional_srcs = conditional_srcs,
        linux_includes = linux_includes,
        hdrs = hdrs,
        copts = copts,
        deps = ["//soc-repo:all_headers"] + ["//motorola/kernel/modules:moto_modules_ddk_headers"] + module_all_deps,
        kernel_build = "//soc-repo:{}_{}_base_kernel".format(build_target, build_variant),
        visibility = ["//visibility:public"]
    )

    copy_to_dist_dir(
        name = "{}_{}_{}_dist".format(build_target, build_variant, name),
        data = [ module_dist_data, ],
        dist_dir = "out/target/product/{}/dlkm/lib/modules/".format(build_target),
        flat = True,
        wipe_dist_dir = False,
        allow_duplicate_filenames = False,
        mode_overrides = {"**/*": "644"},
    )


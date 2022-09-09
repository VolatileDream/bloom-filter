filegroup(
  name = "path_bins",
  srcs = [":bloom-filter"],
  visibility = ["//visibility:public"],
)

cc_binary(
  name = "bloom-filter",
  srcs = [
    "app.c",
    "app.h",
    "main.c",
  ],
  deps = [
    ":libbloom",
    "//third-party:murmurhash3",
  ],
)

cc_library(
  name = "libbloom",
  srcs = ["libbloom.c"],
  hdrs = ["libbloom.h"],
  deps = ["//data/serde"],
)

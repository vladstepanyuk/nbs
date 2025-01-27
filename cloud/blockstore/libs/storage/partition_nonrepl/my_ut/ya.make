UNITTEST_FOR(cloud/blockstore/libs/storage/partition_nonrepl)

INCLUDE(${ARCADIA_ROOT}/cloud/storage/core/tests/recipes/medium.inc)

SRCS(
    part_mirror_split_request_helpers_ut.cpp
)

PEERDIR(
    cloud/blockstore/config
    cloud/blockstore/libs/diagnostics
    cloud/blockstore/libs/rdma_test
    cloud/blockstore/libs/storage/testlib
    cloud/blockstore/libs/storage/disk_agent/actors
)

END()

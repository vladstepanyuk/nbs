#pragma once
#include "public.h"

#include <cloud/blockstore/libs/common/block_range.h>
#include <cloud/blockstore/libs/storage/api/service.h>

#include <contrib/ydb/library/actors/interconnect/types.h>

namespace NCloud::NBlockStore::NStorage::NSplitRequest {

////////////////////////////////////////////////////////////////////////////////

template <typename TMethod>
using TRecordType = TMethod::TRequest::ProtoRecordType;

template <typename TMethod>
struct TRequestToPartitions
{
    TRecordType<TMethod> Request;
    TVector<TActorId> Partitions;
    TBlockRange64 BlockRangeForRequest;

    TRequestToPartitions(
            TRecordType<TMethod> request,
            TVector<TActorId> partitions,
            TBlockRange64 blockRangeForRequest)
        : Request(std::move(request))
        , Partitions(std::move(partitions))
        , BlockRangeForRequest(blockRangeForRequest)
    {}
};

template <typename TMethod>
using TSplittedRequest = TVector<TRequestToPartitions<TMethod>>;

template <typename TMethod>
std::optional<TSplittedRequest<TMethod>> SplitRequest(
    const TRecordType<TMethod>& originalRequest,
    const TVector<TBlockRange64>& blockRangeSplittedByDeviceBorders,
    const TVector<THashSet<TActorId>> partitionsPerDevice)
{
    Y_UNUSED(originalRequest);
    Y_UNUSED(blockRangeSplittedByDeviceBorders);
    Y_UNUSED(partitionsPerDevice);
    static_assert(false, "Not supported method");
}

template <>
std::optional<TSplittedRequest<TEvService::TReadBlocksMethod>> SplitRequest(
    const NProto::TReadBlocksRequest& originalRequest,
    const TVector<TBlockRange64>& blockRangeSplittedByDeviceBorders,
    TVector<THashSet<TActorId>> partitionsPerDevice);

template <>
std::optional<TSplittedRequest<TEvService::TReadBlocksLocalMethod>>
SplitRequest(
    const NProto::TReadBlocksLocalRequest& originalRequest,
    const TVector<TBlockRange64>& blockRangeSplittedByDeviceBorders,
    TVector<THashSet<TActorId>> partitionsPerDevice);

}   // namespace NSplitRequest

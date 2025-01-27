#pragma once
#include "public.h"

#include <cloud/blockstore/libs/common/block_range.h>
#include <cloud/blockstore/libs/storage/api/service.h>

#include <contrib/ydb/library/actors/interconnect/types.h>

namespace NCloud::NBlockStore::NStorage::NSplitRequest {

////////////////////////////////////////////////////////////////////////////////

template <typename TMethod>
using TRequestRecordType = TMethod::TRequest::ProtoRecordType;

template <typename TMethod>
using TResponseRecordType = TMethod::TResponse::ProtoRecordType;

template <typename TMethod>
struct TRequestToPartitions
{
    TRequestRecordType<TMethod> Request;
    TVector<TActorId> Partitions;
    TBlockRange64 BlockRangeForRequest;

    TRequestToPartitions(
            TRequestRecordType<TMethod> request,
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
    const TRequestRecordType<TMethod>& originalRequest,
    const TVector<TBlockRange64>& blockRangeSplittedByDeviceBorders,
    TVector<THashSet<TActorId>> partitionsPerDevice)
{
    Y_UNUSED(originalRequest);
    Y_UNUSED(blockRangeSplittedByDeviceBorders);
    Y_UNUSED(partitionsPerDevice);
    return {};
    // static_assert(false, "Not supported method");
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

template <typename TMethod>
struct TUnifyResponsesContext
{
    TResponseRecordType<TMethod> Response;
    size_t BlocksCountRequested;
};

NProto::TReadBlocksResponse UnifyResponsesRead(
    const TVector<TUnifyResponsesContext<TEvService::TReadBlocksMethod>>&
        responsesToUnify,
    bool fillZeroResponses,
    size_t blockSize);

template <typename TMethod>
TResponseRecordType<TMethod> UnifyResponses(
    const TVector<TUnifyResponsesContext<TMethod>>& responsesToUnify,
    size_t blockSize)
{
    if constexpr (std::is_same_v<TMethod, TEvService::TReadBlocksMethod>) {
        return UnifyResponsesRead(
            std::move(responsesToUnify),
            true,   // fillZeroResponses
            blockSize);
    } else if constexpr (std::is_same_v<TMethod, TEvService::TReadBlocksMethod>)
    {
        return UnifyResponsesRead(
            std::move(responsesToUnify),
            false,   // fillZeroResponses
            blockSize);
    } else {
        return {};
    }
}

}   // namespace NCloud::NBlockStore::NStorage::NSplitRequest

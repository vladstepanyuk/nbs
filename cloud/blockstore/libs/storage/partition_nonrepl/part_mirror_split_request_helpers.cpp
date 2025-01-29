#include "part_mirror_split_request_helpers.h"

#include <ranges>

namespace NCloud::NBlockStore::NStorage::NSplitRequest {

using namespace NActors;

////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename TMethod>
NSplitRequest::TSplittedRequest<TMethod> SplitRequestGeneralRead(
    const TRequestRecordType<TMethod>& originalRequest,
    const TVector<TBlockRange64>& blockRangeSplittedByDeviceBorders,
    TVector<THashSet<TActorId>> partitionsPerDevice)
{
    NSplitRequest::TSplittedRequest<TMethod> result;
    result.reserve(blockRangeSplittedByDeviceBorders.size());

    for (size_t i = 0; i < blockRangeSplittedByDeviceBorders.size(); ++i) {
        const auto& blockRange = blockRangeSplittedByDeviceBorders[i];
        TRequestRecordType<TMethod> copyRequest = originalRequest;
        copyRequest.SetBlocksCount(blockRange.Size());
        copyRequest.SetStartIndex(blockRange.Start);
        TVector<TActorId> partitions(
            std::make_move_iterator(partitionsPerDevice[i].begin()),
            std::make_move_iterator(partitionsPerDevice[i].end()));

        result.push_back(
            {std::move(copyRequest), std::move(partitions), blockRange});
    }

    return result;
}

TStringBuf ConvertBitMapToStringBuf(const TDynBitMap& bitmap)
{
    auto size = bitmap.Size() / 8;
    Y_ABORT_UNLESS(bitmap.GetChunkCount() * sizeof(TDynBitMap::TChunk) == size);
    return {reinterpret_cast<const char*>(bitmap.GetChunks()), size};
}

void ZeroLastNBits(TDynBitMap& bitmap, size_t n)
{
    auto start = n > bitmap.Size() ? 0 : bitmap.Size() - n;
    bitmap.Reset(start, bitmap.Size());
}

}   // namespace

template <>
std::optional<TSplittedRequest<TEvService::TReadBlocksMethod>> SplitRequest(
    const TRequestRecordType<TEvService::TReadBlocksMethod>& originalRequest,
    const TVector<TBlockRange64>& blockRangeSplittedByDeviceBorders,
    TVector<THashSet<TActorId>> partitionsPerDevice)
{
    return SplitRequestGeneralRead<TEvService::TReadBlocksMethod>(
        originalRequest,
        blockRangeSplittedByDeviceBorders,
        std::move(partitionsPerDevice));
}

template <>
std::optional<TSplittedRequest<TEvService::TReadBlocksLocalMethod>>
SplitRequest(
    const TRequestRecordType<TEvService::TReadBlocksLocalMethod>&
        originalRequest,
    const TVector<TBlockRange64>& blockRangeSplittedByDeviceBorders,
    TVector<THashSet<TActorId>> partitionsPerDevice)
{
    auto result = SplitRequestGeneralRead<TEvService::TReadBlocksLocalMethod>(
        originalRequest,
        blockRangeSplittedByDeviceBorders,
        std::move(partitionsPerDevice));

    auto guard = originalRequest.Sglist.Acquire();
    if (!guard) {
        return std::nullopt;
    }

    const auto& originalSglist = guard.Get();
    if (originalSglist.size() == 0) {
        return std::nullopt;
    }

    size_t sglistIdx = 0;
    size_t sglistElementUsedData = 0;
    for (size_t i = 0; i < blockRangeSplittedByDeviceBorders.size(); ++i) {
        TSgList newSglist;
        auto rangeSizeLeft = blockRangeSplittedByDeviceBorders[i].Size() *
                             originalRequest.BlockSize;

        const char* start = nullptr;
        size_t size = 0;

        while (rangeSizeLeft > 0) {
            if (sglistIdx >= originalSglist.size()) {
                return std::nullopt;
            }

            const auto& sglistElement = originalSglist[sglistIdx];
            if (sglistElement.Size() % originalRequest.BlockSize != 0) {
                return std::nullopt;
            }

            if (sglistElement.Size() == sglistElementUsedData) {
                if (size) {
                    newSglist.emplace_back(start, size);
                }
                start = nullptr;
                size = 0;

                ++sglistIdx;
                sglistElementUsedData = 0;

                continue;
            }

            if (!start) {
                start = sglistElement.Data() + sglistElementUsedData;
            }

            auto nextSglistElementUsedData =
                Min(sglistElement.Size(),
                    sglistElementUsedData + rangeSizeLeft);
            size = nextSglistElementUsedData - sglistElementUsedData;
            rangeSizeLeft -= size;
            sglistElementUsedData = nextSglistElementUsedData;
        }

        if (size) {
            newSglist.emplace_back(start, size);
        }

        result[i].Request.Sglist =
            originalRequest.Sglist.Create(std::move(newSglist));
    }

    if (sglistIdx != originalSglist.size() - 1 ||
        originalSglist.back().Size() != sglistElementUsedData)
    {
        return std::nullopt;
    }

    return result;
}

NProto::TReadBlocksResponse UnifyResponsesRead(
    const TVector<TUnifyResponsesContext<TEvService::TReadBlocksMethod>>&
        responsesToUnify,
    bool fillZeroResponses,
    size_t blockSize)
{
    NProto::TReadBlocksResponse result;

    const auto* errorResponse = FindIfPtr(
        responsesToUnify,
        [&](const auto& el) { return HasError(el.Response.GetError()); });

    if (errorResponse) {
        result.MutableError()->CopyFrom(errorResponse->Response.GetError());

        return result;
    }

    size_t overallSize = 0;
    for (const auto& [_, blocksRequested]: responsesToUnify) {
        overallSize += blocksRequested;
    }

    TDynBitMap map;
    map.Reserve(overallSize);
    map.Clear();

    TDynBitMap tmpMap;

    for (const auto& responseCtx: responsesToUnify | std::views::reverse) {
        map.LShift(responseCtx.BlocksCountRequested);

        tmpMap.Reserve(responseCtx.BlocksCountRequested);
        tmpMap.Clear();
        auto* dst = const_cast<TDynBitMap::TChunk*>(tmpMap.GetChunks());
        const auto& bitBufferToAdd =
            responseCtx.Response.GetUnencryptedBlockMask();
        memcpy(
            dst,
            bitBufferToAdd.Data(),
            Min(bitBufferToAdd.Size(),
                tmpMap.GetChunkCount() * sizeof(TDynBitMap::TChunk)));
        ZeroLastNBits(tmpMap, tmpMap.Size() - responseCtx.BlocksCountRequested);

        map.Or(tmpMap);
    }
    result.MutableUnencryptedBlockMask()->assign(ConvertBitMapToStringBuf(map));

    ui64 throttlerDelayMax = 0;
    bool allZeros = true;
    bool allBlocksEmpty = true;
    for (const auto& [response, blocksCountRequested]: responsesToUnify) {
        allZeros &= response.GetAllZeroes();
        allBlocksEmpty &= response.GetBlocks().BuffersSize() == 0;
        throttlerDelayMax =
            Max(throttlerDelayMax, response.GetThrottlerDelay());
    }

    result.SetThrottlerDelay(throttlerDelayMax);
    result.SetAllZeroes(allZeros);

    if (allBlocksEmpty) {
        return result;
    }
    for (const auto& [response, blocksCountRequested]: responsesToUnify) {
        auto blocks = response.GetBlocks();

        if (blocks.BuffersSize() == 0 && fillZeroResponses) {
            for (size_t i = 0; i < blocksCountRequested; ++i) {
                result.MutableBlocks()->AddBuffers(TString(blockSize, '\0'));
            }
        } else {
            for (auto buffer: blocks.GetBuffers()) {
                result.MutableBlocks()->AddBuffers(std::move(buffer));
            }
        }
    }

    return result;
}
}   // namespace NCloud::NBlockStore::NStorage::NSplitRequest

#include "part_mirror_split_requests_utils.h"


namespace NCloud::NBlockStore::NStorage::NSplitRequest {

////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename TMethod>
NSplitRequest::TSplittedRequest<TMethod> SplitRequestGeneralRead(
    const NSplitRequest::TRecordType<TMethod>& originalRequest,
    const TVector<TBlockRange64>& blockRangeSplittedByDeviceBorders,
    TVector<THashSet<TActorId>> partitionsPerDevice)
{
    NSplitRequest::TSplittedRequest<TMethod> result;
    result.reserve(blockRangeSplittedByDeviceBorders.size());

    for (size_t i = 0; i < blockRangeSplittedByDeviceBorders.size(); ++i) {
        const auto& blockRange = blockRangeSplittedByDeviceBorders[i];
        NSplitRequest::TRecordType<TMethod> copyRequest = originalRequest;
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

}   // namespace

template <>
std::optional<TSplittedRequest<TEvService::TReadBlocksMethod>> SplitRequest(
    const TRecordType<TEvService::TReadBlocksMethod>& originalRequest,
    const TVector<TBlockRange64>& blockRangeSplittedByDeviceBorders,
    TVector<THashSet<TActorId>> partitionsPerDevice)
{
    return SplitRequestGeneralRead<TEvService::TReadBlocksMethod>(
        originalRequest,
        blockRangeSplittedByDeviceBorders, std::move(partitionsPerDevice));
}

template <>
std::optional<TSplittedRequest<TEvService::TReadBlocksLocalMethod>>
SplitRequest(
    const TRecordType<TEvService::TReadBlocksLocalMethod>& originalRequest,
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
            if (sglistIdx < originalSglist.size()) {
                return std::nullopt;
            }

            const auto& sglistElement = originalSglist[sglistIdx];
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
            rangeSizeLeft -=
                (nextSglistElementUsedData - sglistElementUsedData);
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

}   // namespace NCloud::NBlockStore::NStorage::NSplitRequest

#include "volume_actor.h"

#include <cloud/storage/core/libs/common/media.cpp>
namespace NCloud::NBlockStore::NStorage {

using namespace NActors;

using namespace NKikimr::NTabletFlatExecutor;

namespace {
class TDumpChannelsGroupsIdsActor
    : TActorBootstrapped<TDumpChannelsGroupsIdsActor>
{
    const TString DiskId;
    const TVector<TActorId> Partitions;
    const TRequestInfoPtr RequestInfo;

    TVector<ui32> AllGroups;
    ui32 PendingRequests = 0;

public:
    explicit TDumpChannelsGroupsIdsActor(
        TString diskId,
        TVector<TActorId> partitions,
        TRequestInfoPtr requestInfo);

    void Bootstrap(const TActorContext& ctx);

private:
    void ReplyAndDie(const TActorContext& ctx, NProto::TError error);

private:
    STFUNC(StateWork);

    void HandlePoisonPill(
        const TEvents::TEvPoisonPill::TPtr& ev,
        const TActorContext& ctx);

    void HandlePartitionResponse(
        const TEvVolume::TEvDumpChannelsGroupsResponse::TPtr& ev,
        const TActorContext& ctx);
};

TDumpChannelsGroupsIdsActor::TDumpChannelsGroupsIdsActor(
    TString diskId,
    TVector<TActorId> partitions,
    TRequestInfoPtr requestInfo)
    : DiskId(std::move(diskId))
    , Partitions(std::move(partitions))
    , RequestInfo(std::move(requestInfo))
{}

void TDumpChannelsGroupsIdsActor::Bootstrap(const TActorContext& ctx)
{
    for (const auto& part: Partitions) {
        NProto::TDumpChannelsGroupsRequest request;

        request.SetDiskId(DiskId);

        NCloud::Send(
            ctx,
            part,
            std::make_unique<TEvVolume::TEvDumpChannelsGroupsRequest>(
                std::move(request)));
        ++PendingRequests;
    }

    Become(&TThis::StateWork);
}

void TDumpChannelsGroupsIdsActor::ReplyAndDie(
    const TActorContext& ctx,
    NProto::TError error)
{
    if (HasError(error)) {
        NCloud::Reply(
            ctx,
            *RequestInfo,
            std::make_unique<TEvVolume::TEvDumpChannelsGroupsResponse>(
                std::move(error)));

        Die(ctx);
        return;
    }

    NProto::TDumpChannelsGroupsResponse response;

    Die(ctx);
}

STFUNC(TDumpChannelsGroupsIdsActor::StateWork)
{
    switch (ev->GetTypeRewrite()) {
        HFunc(TEvents::TEvPoisonPill, HandlePoisonPill);

        HFunc(
            TEvVolume::TEvDumpChannelsGroupsResponse,
            HandlePartitionResponse);

            // HFunc(
            //     TEvDiskAgent::TEvAcquireDevicesResponse,
            //     HandleAcquireDevicesResponse);
            // HFunc(
            //     TEvDiskAgent::TEvAcquireDevicesRequest,
            //     HandleAcquireDevicesUndelivery);

            // HFunc(TEvents::TEvWakeup, HandleWakeup);

        default:
            HandleUnexpectedEvent(ev, TBlockStoreComponents::VOLUME);
            break;
    }
}

void TDumpChannelsGroupsIdsActor::HandlePoisonPill(
    const TEvents::TEvPoisonPill::TPtr& ev,
    const TActorContext& ctx)
{
    Y_UNUSED(ev);
    ReplyAndDie(ctx, MakeTabletIsDeadError(E_REJECTED, __LOCATION__));
}

void TDumpChannelsGroupsIdsActor::HandlePartitionResponse(
    const TEvVolume::TEvDumpChannelsGroupsResponse::TPtr& ev,
    const TActorContext& ctx)
{
    auto* msg = ev->Get();

    if (HasError(msg->GetError())) {
        for (const auto& groupId: msg->Record.GetGroupIds()) {
            AllGroups.emplace_back(groupId);
        }
    }
    if (--PendingRequests == 0) {
        ReplyAndDie(ctx, {});
    }
}

}   // namespace

void TVolumeActor::HandleDumpChannelsGroups(
    const TEvVolume::TEvDumpChannelsGroupsRequest::TPtr& ev,
    const NActors::TActorContext& ctx)
{
    if (IsDiskRegistryMediaKind(
            State->GetMeta().GetConfig().GetStorageMediaKind()))
    {
        NCloud::Reply(
            ctx,
            *ev,
            std::make_unique<TEvVolume::TEvDumpChannelsGroupsRequest>(MakeError(
                E_NOT_IMPLEMENTED,
                "request supported only for replicated volumes")));
        return;
    }

    State->GetPartitions();
    // NCloud::Send(ctx, )
}

}   // namespace NCloud::NBlockStore::NStorage

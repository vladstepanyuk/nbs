#pragma once
#include "defs.h"

namespace NKikimr {
namespace NDataShard {

enum class EExecutionUnitKind : ui32 {
    CheckDataTx,
    CheckSchemeTx,
    CheckSnapshotTx,
    CheckDistributedEraseTx,
    CheckCommitWritesTx,
    CheckRead,
    StoreDataTx,
    StoreSchemeTx,
    StoreSnapshotTx,
    StoreDistributedEraseTx,
    StoreCommitWritesTx,
    BuildAndWaitDependencies,
    FinishPropose,
    CompletedOperations,
    WaitForPlan,
    PlanQueue,
    LoadTxDetails,
    FinalizeDataTxPlan,
    ProtectSchemeEchoes,
    BuildDataTxOutRS,
    BuildKqpDataTxOutRS,
    BuildDistributedEraseTxOutRS,
    StoreAndSendOutRS,
    PrepareDataTxInRS,
    PrepareKqpDataTxInRS,
    PrepareDistributedEraseTxInRS,
    LoadAndWaitInRS,
    ExecuteDataTx,
    ExecuteKqpDataTx,
    ExecuteDistributedEraseTx,
    ExecuteCommitWritesTx,
    ExecuteRead,
    CompleteOperation,
    ExecuteKqpScanTx,
    MakeScanSnapshot,
    WaitForStreamClearance,
    ReadTableScan,
    MakeSnapshot,
    BuildSchemeTxOutRS,
    PrepareSchemeTxInRS,
    Backup,
    Restore,
    CreateTable,
    ReceiveSnapshot,
    ReceiveSnapshotCleanup,
    AlterMoveShadow,
    AlterTable,
    DropTable,
    DirectOp,
    CreatePersistentSnapshot,
    DropPersistentSnapshot,
    CreateVolatileSnapshot,
    DropVolatileSnapshot,
    InitiateBuildIndex,
    FinalizeBuildIndex,
    DropIndexNotice,
    MoveTable,
    CreateCdcStream,
    AlterCdcStream,
    DropCdcStream,
    MoveIndex,
    Count,
    Unspecified
};

} // namespace NDataShard
} // namespace NKikimr
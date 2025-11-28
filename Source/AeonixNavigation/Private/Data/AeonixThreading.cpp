// Copyright 2024 Chris Ashworth

#include "Data/AeonixThreading.h"
#include "AeonixNavigation.h"
#include "HAL/PlatformProcess.h"

// FAeonixPathfindWorker implementation

FAeonixPathfindWorker::FAeonixPathfindWorker(
	TQueue<TFunction<void()>, EQueueMode::Spsc>* InWorkQueue,  // SPSC - single consumer!
	FEvent* InWorkAvailableEvent,
	FThreadSafeBool* InShuttingDown,
	int32 InWorkerIndex)
	: WorkQueue(InWorkQueue)
	, WorkAvailableEvent(InWorkAvailableEvent)
	, bShuttingDown(InShuttingDown)
	, WorkerIndex(InWorkerIndex)
	, bStopRequested(false)
{
}

FAeonixPathfindWorker::~FAeonixPathfindWorker()
{
}

bool FAeonixPathfindWorker::Init()
{
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Pathfind worker %d initialized"), WorkerIndex);
	return true;
}

uint32 FAeonixPathfindWorker::Run()
{
	UE_LOG(LogAeonixNavigation, Log, TEXT("Pathfind worker %d starting"), WorkerIndex);

	while (!(*bShuttingDown) && !bStopRequested)
	{
		// Wait for work to become available
		WorkAvailableEvent->Wait(100); // 100ms timeout to check shutdown flag

		// Process all available work
		TFunction<void()> WorkItem;
		while (WorkQueue->Dequeue(WorkItem))
		{
			if (*bShuttingDown || bStopRequested)
			{
				break;
			}

			// Execute the work item
			if (WorkItem)
			{
				WorkItem();
			}
		}
	}

	UE_LOG(LogAeonixNavigation, Log, TEXT("Pathfind worker %d stopping"), WorkerIndex);
	return 0;
}

void FAeonixPathfindWorker::Stop()
{
	bStopRequested = true;
	if (WorkAvailableEvent)
	{
		WorkAvailableEvent->Trigger();
	}
}

void FAeonixPathfindWorker::Exit()
{
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Pathfind worker %d exiting"), WorkerIndex);
}

// FAeonixPathfindWorkerPool implementation

FAeonixPathfindWorkerPool::FAeonixPathfindWorkerPool()
	: bShuttingDown(false)
	, bInitialized(false)
{
}

FAeonixPathfindWorkerPool::~FAeonixPathfindWorkerPool()
{
	Shutdown();
}

void FAeonixPathfindWorkerPool::Initialize(int32 NumThreads)
{
	if (bInitialized)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Worker pool already initialized"));
		return;
	}

	// Validate thread count
	const int32 MaxThreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads() - 2;
	NumThreads = FMath::Clamp(NumThreads, 1, FMath::Max(1, MaxThreads));

	UE_LOG(LogAeonixNavigation, Log, TEXT("Initializing pathfind worker pool with %d threads (per-worker SPSC queues)"), NumThreads);

	// Pre-allocate worker contexts
	WorkerContexts.SetNum(NumThreads);

	// Create per-worker contexts (each with its own SPSC queue and event)
	for (int32 i = 0; i < NumThreads; ++i)
	{
		FAeonixWorkerContext& Context = WorkerContexts[i];

		// Per-worker event
		Context.WorkAvailableEvent = FPlatformProcess::GetSynchEventFromPool(false);

		// Per-worker worker (references its own SPSC queue)
		Context.Worker = MakeUnique<FAeonixPathfindWorker>(
			&Context.WorkQueue,
			Context.WorkAvailableEvent,
			&bShuttingDown,
			i);

		// Per-worker thread (raw pointer - Kill() handles cleanup)
		Context.Thread = FRunnableThread::Create(
			Context.Worker.Get(),
			*FString::Printf(TEXT("AeonixPathfindWorker_%d"), i),
			0, // Default stack size
			TPri_BelowNormal);

		if (Context.Thread)
		{
			UE_LOG(LogAeonixNavigation, Verbose, TEXT("Created pathfind worker thread %d with dedicated SPSC queue"), i);
		}
		else
		{
			UE_LOG(LogAeonixNavigation, Error, TEXT("Failed to create pathfind worker thread %d"), i);
		}
	}

	bInitialized = true;
	UE_LOG(LogAeonixNavigation, Log, TEXT("Pathfind worker pool initialized with %d threads"), WorkerContexts.Num());
}

void FAeonixPathfindWorkerPool::EnqueueWork(TFunction<void()>&& Work)
{
	if (!bInitialized)
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Attempted to enqueue work to uninitialized worker pool"));
		return;
	}

	if (bShuttingDown)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Attempted to enqueue work to shutting down worker pool"));
		return;
	}

	if (WorkerContexts.Num() == 0)
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Worker pool has no workers"));
		return;
	}

	// Round-robin distribution: atomically increment and wrap
	const uint32 Index = NextWorkerIndex.fetch_add(1, std::memory_order_relaxed);
	const int32 WorkerIndex = Index % WorkerContexts.Num();

	FAeonixWorkerContext& Context = WorkerContexts[WorkerIndex];
	Context.WorkQueue.Enqueue(MoveTemp(Work));
	Context.WorkAvailableEvent->Trigger();
}

void FAeonixPathfindWorkerPool::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	UE_LOG(LogAeonixNavigation, Log, TEXT("Shutting down pathfind worker pool with %d threads"), WorkerContexts.Num());

	bShuttingDown = true;

	// Wake all workers so they can exit
	for (FAeonixWorkerContext& Context : WorkerContexts)
	{
		if (Context.WorkAvailableEvent)
		{
			Context.WorkAvailableEvent->Trigger();
		}
	}

	// Kill and delete all threads (Kill waits for completion internally)
	for (FAeonixWorkerContext& Context : WorkerContexts)
	{
		if (Context.Thread)
		{
			Context.Thread->Kill(true);  // bShouldWait = true
			delete Context.Thread;
			Context.Thread = nullptr;
		}
	}

	// Return events to pool
	for (FAeonixWorkerContext& Context : WorkerContexts)
	{
		if (Context.WorkAvailableEvent)
		{
			FPlatformProcess::ReturnSynchEventToPool(Context.WorkAvailableEvent);
			Context.WorkAvailableEvent = nullptr;
		}
	}

	// Clear contexts (releases Workers and Threads)
	WorkerContexts.Empty();

	bInitialized = false;
	UE_LOG(LogAeonixNavigation, Log, TEXT("Pathfind worker pool shutdown complete"));
}

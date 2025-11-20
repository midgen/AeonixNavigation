// Copyright 2024 Chris Ashworth

#include "Data/AeonixThreading.h"
#include "AeonixNavigation.h"
#include "HAL/PlatformProcess.h"

// FAeonixPathfindWorker implementation

FAeonixPathfindWorker::FAeonixPathfindWorker(
	TQueue<TFunction<void()>, EQueueMode::Mpsc>* InWorkQueue,
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
	, WorkAvailableEvent(nullptr)
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

	UE_LOG(LogAeonixNavigation, Log, TEXT("Initializing pathfind worker pool with %d threads"), NumThreads);

	// Create event for signaling work availability
	WorkAvailableEvent = FPlatformProcess::GetSynchEventFromPool(false);

	// Create worker threads
	WorkerThreads.Reserve(NumThreads);
	for (int32 i = 0; i < NumThreads; ++i)
	{
		FAeonixPathfindWorker* Worker = new FAeonixPathfindWorker(
			&WorkQueue,
			WorkAvailableEvent,
			&bShuttingDown,
			i);

		FRunnableThread* Thread = FRunnableThread::Create(
			Worker,
			*FString::Printf(TEXT("AeonixPathfindWorker_%d"), i),
			0, // Default stack size
			TPri_BelowNormal);

		if (Thread)
		{
			TUniquePtr<FRunnableThread> ThreadPtr(Thread);
			WorkerThreads.Add(MoveTemp(ThreadPtr));
			UE_LOG(LogAeonixNavigation, Verbose, TEXT("Created pathfind worker thread %d"), i);
		}
		else
		{
			UE_LOG(LogAeonixNavigation, Error, TEXT("Failed to create pathfind worker thread %d"), i);
			delete Worker;
		}
	}

	bInitialized = true;
	UE_LOG(LogAeonixNavigation, Log, TEXT("Pathfind worker pool initialized with %d threads"), WorkerThreads.Num());
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

	WorkQueue.Enqueue(MoveTemp(Work));
	WorkAvailableEvent->Trigger();
}

void FAeonixPathfindWorkerPool::Shutdown()
{
	if (!bInitialized)
	{
		return;
	}

	UE_LOG(LogAeonixNavigation, Log, TEXT("Shutting down pathfind worker pool with %d threads"), WorkerThreads.Num());

	bShuttingDown = true;
	WorkAvailableEvent->Trigger();

	// Wait for all threads to complete
	for (TUniquePtr<FRunnableThread>& ThreadPtr : WorkerThreads)
	{
		if (ThreadPtr.IsValid())
		{
			ThreadPtr->WaitForCompletion();
		}
	}

	WorkerThreads.Empty();

	// Return event to pool
	if (WorkAvailableEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(WorkAvailableEvent);
		WorkAvailableEvent = nullptr;
	}

	bInitialized = false;
	UE_LOG(LogAeonixNavigation, Log, TEXT("Pathfind worker pool shutdown complete"));
}

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

namespace Turso3D
{
	// Task for execution by worker threads.
	struct Task
	{
		// Default-construct.
		Task();
		// Destruct.
		virtual ~Task();

		// Call the work function.
		// Thread index 0 is the main thread.
		virtual void Complete(unsigned threadIndex) = 0;

		// Dependent tasks.
		std::vector<Task*> dependentTasks;
		// Dependency counter.
		// Once zero, this task will be automatically queue itself.
		std::atomic<int> numDependencies;
	};

	// Free function task.
	struct FunctionTask : public Task
	{
		typedef void (*WorkFunctionPtr)(Task*, unsigned);

		// Construct.
		FunctionTask(WorkFunctionPtr function_) :
			function(function_)
		{
		}

		// Call the work function.
		// Thread index 0 is the main thread.
		void Complete(unsigned threadIndex) override
		{
			function(this, threadIndex);
		}

		// Task function.
		WorkFunctionPtr function;
	};

	// Member function task.
	template <class T>
	struct MemberFunctionTask : public Task
	{
		typedef void (T::* MemberWorkFunctionPtr)(Task*, unsigned);

		// Construct.
		MemberFunctionTask(T* object_, MemberWorkFunctionPtr function_) :
			object(object_),
			function(function_)
		{
		}

		// Call the work function.
		// Thread index 0 is the main thread.
		void Complete(unsigned threadIndex) override
		{
			(object->*function)(this, threadIndex);
		}

		// Object instance.
		T* object;
		// Task member function.
		MemberWorkFunctionPtr function;
	};

	// ==========================================================================================
	// Worker thread subsystem for dividing tasks between CPU cores.
	class WorkQueue
	{
	public:
		// Construct
		WorkQueue();
		// Destruct.
		// Stop worker threads.
		~WorkQueue();

		// Optional.
		// Create the specified number of worker threads.
		// A value of 0 will use the value of std::thread::hardware_concurrency()
		// for the number of worker threads to be created, but will cap at 16.
		void CreateWorkerThreads(unsigned numThreads);

		// Queue a task for execution.
		// If no worker threads, completes immediately in the main thread.
		void QueueTask(Task* task);
		// Queue several tasks execution.
		// If no worker threads, completes immediately in the main thread.
		void QueueTasks(size_t count, Task** tasks);
		// Add a dependency to a task.
		// These tasks should not be queued via QueueTask(), they will instead queue themselves when the dependencies have finished.
		void AddDependency(Task* task, Task* dependency);
		// Complete all currently queued tasks and tasks with dependencies.
		// To be called only from the main thread.
		// Ensure that all dependencies either have been queued or will be queued by other tasks, otherwise this function never returns.
		void Complete();
		// Execute a task from the queue if available, then return.
		// To be called only from the main thread.
		// Return true if a task was executed.
		bool TryComplete();

		// Return number of execution threads including the main thread.
		unsigned NumThreads() const { return (unsigned)threads.size() + 1; }

		// Return thread index when outside of a work function.
		static unsigned ThreadIndex() { return threadIndex; }

	private:
		// Worker thread function.
		void WorkerLoop(unsigned threadIndex);
		// Complete a task by calling its work function and signal dependents.
		void CompleteTask(Task*, unsigned threadIndex);

	private:
		// Mutex for the work queue.
		std::mutex queueMutex;
		// Condition variable to wake up workers.
		std::condition_variable signal;
		// Exit flag.
		volatile bool shouldExit;
		// Task queue.
		std::queue<Task*> tasks;
		// Worker threads.
		std::vector<std::thread> threads;
		// Amount of tasks in queue.
		std::atomic<int> numQueuedTasks;
		// Amount of queued tasks. Used to check for completion.
		std::atomic<int> numPendingTasks;

		// Thread index for queries outside the work functions.
		static thread_local unsigned threadIndex;
	};
}

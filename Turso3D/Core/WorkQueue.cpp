#include <Turso3D/Core/WorkQueue.h>
#include <Turso3D/IO/Log.h>
#include <cassert>
#include <string>

namespace Turso3D
{
	thread_local unsigned WorkQueue::threadIndex = 0;

	// ==========================================================================================
	Task::Task()
	{
		numDependencies.store(0);
	}

	Task::~Task()
	{
	}

	// ==========================================================================================
	WorkQueue::WorkQueue() :
		shouldExit(false)
	{
		numQueuedTasks.store(0);
		numPendingTasks.store(0);

		Log::ThreadName().assign("MainThread");
	}

	WorkQueue::~WorkQueue()
	{
		if (!threads.size()) {
			return;
		}

		// Signal exit and wait for threads to finish
		shouldExit = true;

		signal.notify_all();
		for (std::thread& thread : threads) {
			thread.join();
		}
	}

	void WorkQueue::CreateWorkerThreads(unsigned numThreads)
	{
		// Exit all current worker threads (if any)
		if (!threads.empty()) {
			LOG_INFO("Finalizing {} worker threads.", threads.size());

			// Signal exit and wait for threads to finish
			shouldExit = true;
			signal.notify_all();
			for (std::thread& thread : threads) {
				thread.join();
			}
			threads.clear();
		}

		// Limit work threads
		unsigned max_threads = std::thread::hardware_concurrency();
		numThreads = std::min(std::min(numThreads, max_threads), 8u);

		LOG_INFO("Creating {} worker threads.", numThreads);

		for (unsigned i = 0; i < numThreads; ++i) {
			std::thread& worker = threads.emplace_back(std::thread(&WorkQueue::WorkerLoop, this, i + 1));
		}
	}

	void WorkQueue::QueueTask(Task* task)
	{
		assert(task);
		assert(task->numDependencies.load() == 0);

		if (threads.size()) {
			assert(task->numDependencies.load() == 0);
			{
				std::lock_guard<std::mutex> lock(queueMutex);
				tasks.push(task);
			}

			numQueuedTasks.fetch_add(1);
			numPendingTasks.fetch_add(1);

			signal.notify_one();
		} else {
			// If no threads, execute directly
			CompleteTask(task, 0);
		}
	}

	void WorkQueue::QueueTasks(size_t count, Task** tasks_)
	{
		if (threads.size()) {
			{
				std::lock_guard<std::mutex> lock(queueMutex);
				for (size_t i = 0; i < count; ++i) {
					assert(tasks_[i]);
					assert(tasks_[i]->numDependencies.load() == 0);
					tasks.push(tasks_[i]);
				}
			}

			numQueuedTasks.fetch_add((int)count);
			numPendingTasks.fetch_add((int)count);

			if (count >= threads.size()) {
				signal.notify_all();
			} else {
				for (size_t i = 0; i < count; ++i) {
					signal.notify_one();
				}
			}
		} else {
			// If no threads, execute directly
			for (size_t i = 0; i < count; ++i) {
				CompleteTask(tasks_[i], 0);
			}
		}
	}

	void WorkQueue::AddDependency(Task* task, Task* dependency)
	{
		assert(task);
		assert(dependency);

		dependency->dependentTasks.push_back(task);

		// If this is the first dependency added, increment the global pending task counter so we know to wait for the dependent task in Complete().
		if (task->numDependencies.fetch_add(1) == 0) {
			numPendingTasks.fetch_add(1);
		}
	}

	void WorkQueue::Complete()
	{
		if (!threads.size()) {
			return;
		}

		for (;;) {
			if (!numPendingTasks.load()) {
				break;
			}

			// Avoid locking the queue mutex if do not have tasks in queue, just wait for the workers to finish
			if (!numQueuedTasks.load()) {
				continue;
			}

			// Otherwise if have still tasks, execute them in the main thread
			Task* task;

			{
				std::lock_guard<std::mutex> lock(queueMutex);
				if (!tasks.size()) {
					continue;
				}

				task = tasks.front();
				tasks.pop();
			}

			numQueuedTasks.fetch_add(-1);
			CompleteTask(task, 0);
		}
	}

	bool WorkQueue::TryComplete()
	{
		if (!threads.size() || !numPendingTasks.load() || !numQueuedTasks.load()) {
			return false;
		}

		Task* task;

		{
			std::lock_guard<std::mutex> lock(queueMutex);
			if (!tasks.size()) {
				return false;
			}

			task = tasks.front();
			tasks.pop();
		}

		numQueuedTasks.fetch_add(-1);
		CompleteTask(task, 0);

		return true;
	}

	// ==========================================================================================
	void WorkQueue::WorkerLoop(unsigned threadIndex_)
	{
		WorkQueue::threadIndex = threadIndex_;

		Log::ThreadName().assign(fmt::format("WorkQueue{:d}", threadIndex_));

		for (;;) {
			Task* task;
			{
				std::unique_lock<std::mutex> lock(queueMutex);
				signal.wait(lock, [this]
				{
					return !tasks.empty() || shouldExit;
				});

				if (shouldExit) {
					break;
				}

				task = tasks.front();
				tasks.pop();
			}

			numQueuedTasks.fetch_add(-1);
			CompleteTask(task, threadIndex_);
		}
	}

	void WorkQueue::CompleteTask(Task* task, unsigned threadIndex_)
	{
		task->Complete(threadIndex_);

		if (task->dependentTasks.size()) {
			// Queue dependent tasks now if no more dependencies left
			for (auto it = task->dependentTasks.begin(); it != task->dependentTasks.end(); ++it) {
				Task* dependentTask = *it;

				if (dependentTask->numDependencies.fetch_add(-1) == 1) {
					if (threads.size()) {
						{
							std::lock_guard<std::mutex> lock(queueMutex);
							tasks.push(dependentTask);
						}

						// Note: numPendingTasks counter was already incremented when adding the first dependency, do not do again here
						numQueuedTasks.fetch_add(1);

						signal.notify_one();
					} else {
						// If no threads, execute directly
						CompleteTask(dependentTask, 0);
					}
				}
			}

			task->dependentTasks.clear();
		}

		// Decrement pending task counter last, so that WorkQueue::Complete() will also wait for the potentially added dependent tasks
		numPendingTasks.fetch_add(-1);
	}
}

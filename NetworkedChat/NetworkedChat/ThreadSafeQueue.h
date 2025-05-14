#pragma once
#pragma once
#include <queue>
#include <mutex>

// Class for a thread-safe queue, to be used in a thread pool
template <typename T>
class ThreadSafeQueue
{
public:
	ThreadSafeQueue() = default;
	~ThreadSafeQueue()
	{
		while (!m_Queue.empty())
		{
			m_Queue.pop();
		}
	}

	void Push(const T& _item);
	bool BlockPop(T& _item);
	void UnblockAll();
	std::queue<T> m_Queue;

private:
	std::mutex m_Mutex;
	std::condition_variable m_ConVar;
	std::atomic_bool m_Escape;
};

// Add to queue and allow next thread to access
template <typename T>
inline void ThreadSafeQueue<T>::Push(const T& _item)
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	m_Queue.push(_item);
	m_ConVar.notify_one();
}

// One thread at a time access to remove an item from queue
template <typename T>
inline bool ThreadSafeQueue<T>::BlockPop(T& _item)
{
	std::unique_lock<std::mutex> lock(m_Mutex);

	m_ConVar.wait(lock, [this] {
		return (m_Queue.empty() == false || m_Escape);
		});

	if (!m_Escape)
	{
		_item = std::move(m_Queue.front());
		m_Queue.pop();
	}
	return !m_Escape;
}

// Notify all threads and continue through block
template <typename T>
inline void ThreadSafeQueue<T>::UnblockAll()
{
	m_Escape = true;
	m_ConVar.notify_all();
}
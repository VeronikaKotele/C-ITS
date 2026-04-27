#pragma once
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <numeric>
#include <concepts>

template <typename Message>
class ProcessMessageQueue
{
private:
	struct PrioritizedMessage {
		Message message;
		uint32_t deadline;
	};

	struct PriorityComparator {
		bool operator()(const PrioritizedMessage& a, const PrioritizedMessage& b) const {
			return a.deadline > b.deadline; // earlier deadline has higher priority
		}
	};

public:
	ProcessMessageQueue() = default;

	bool isEmpty() const {
		std::lock_guard<std::mutex> lock(_inboxMutex);
		return _queue.empty();
	}

	void add(Message message, uint32_t deadline) {
		{
			std::lock_guard<std::mutex> lock(_inboxMutex);
			_queue.emplace(message, deadline);
		}

		_inboxCv.notify_one();
	}

	template <typename Callback> requires std::invocable<Callback&, const Message&>
	void startProcessingThread(Callback callback) {
		_running = true;

		_workerThread = std::thread([this, callback]() {
			while (_running) {
				Message msg;
				{
					std::unique_lock<std::mutex> lock(_inboxMutex);

					_inboxCv.wait(lock, [this]() {
						return !_queue.empty() || !_running;
					});

					if (!_running && _queue.empty()) {
						break;
					}

					msg = std::move(_queue.top().message);
					_queue.pop();
				}

				try {
					callback(msg);
				}
				catch (const std::exception& ex) {
					std::cerr << "Exception in ProcessMessageQueue callback: " << ex.what() << "\n";
				}
			}
		});
	}

	void stopProcessingThread() {
		_running = false;
		_inboxCv.notify_all();

		if (_workerThread.joinable()) {
			_workerThread.join();
		}
	}

private:
	std::mutex _inboxMutex;
	std::condition_variable _inboxCv;
	std::priority_queue<PrioritizedMessage, std::vector<PrioritizedMessage>, PriorityComparator> _queue;
	std::thread _workerThread;
	std::atomic<bool> _running{ false };
};
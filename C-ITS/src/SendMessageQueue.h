#pragma once
#include <vector>
#include <numeric>
#include <concepts>

template <typename Message>
class SendMessageQueue
{
private:
	struct PrioritizedMessage {
		Message message;
		uint32_t deadline;
	};

	struct PriorityComparator {
		bool operator()(const PrioritizedMessage& a, const PrioritizedMessage& b) const {
			return a.deadline < b.deadline; // earlier deadline has higher priority
		}
	};

public:
	SendMessageQueue() = default;

	void add(Message message, uint32_t deadline) {
		_queue.emplace(message, deadline);
	}

	template <typename Callback >
	requires std::invocable<Callback&, const Message&>
	bool sendTopPriority(Callback callback) {
		if (!_queue.empty()) {
			Message msg = _queue.top().message;
			_queue.pop();
			try {
				callback(msg);
			}
			catch (const std::exception& ex) {
				std::cerr << "Exception in SendMessageQueue callback: " << ex.what() << "\n";
			}
			return true;
		}

		return false;
	}

private:
	std::priority_queue<PrioritizedMessage, std::vector<PrioritizedMessage>, PriorityComparator> _queue;
};
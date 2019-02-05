#include "matchers/simplematcher.h"

namespace exampi
{

SimpleMatcher::SimpleMatcher() : new_receives(0)
{
	;
}

void SimpleMatcher::post_request(Request_ptr request)
{
	std::lock_guard<std::recursive_mutex> lock(guard);

	posted_request_queue.push_back(request);
}

bool match(ProtocolMessage_uptr message, Match &match)
{
	std::lock_guard<std::recursive_mutex> lock(guard);

	// do search
	auto iterator = std::find_if(posted_request_queue.begin(),
	                             posted_request_queue.end(),
	                             [message](const Request_ptr request) -> bool
	                             {
									// TODO support WILD CARD ANY_TAG AND ANY_SOURCE
	                                return (message->envelope.epoch        == request->envelope.epoch) &&
	                                       (message->envelope.communicator == request->envelope.communicator) &&
	                                       (message->envelope.source       == request->envelope.source) &&
	                                       (message->envelope.destination  == request->envelope.destination) &&
	                                       (message->envelope.tag          == request->envelope.tag);
	                             });

	// found match
	if(bool matched = (iterator != posted_request_queue.end()))
	{
		// return corresponding request
		Request_ptr request = *iterator;
		
		// remove request from prq
		posted_request_queue.remove(request);

		// return match
		match.request = request;
		match.message = std::move(message);
	}
	
	// no match, then store message in UMQ
	else
	{
		unexpected_message_queue.push_back(std::move(message));
	}
	
	return matched;
}

bool SimpleMatcher::progress(Match &match)
{
	std::lock_guard<std::recursive_mutex> lock(guard);
	
	// check if work is actually available
	if(has_work() && (unexpected_message_queue.size() > 0))
		// with multiple threads need to keep in mind FIFO, single lock works
		ProtocolMessage_uptr message = std::move(unexpected_message_queue.front());
		unexpected_message_queue.pop();

		// TODO this needs improving

		bool matched = match(message, match);
		if(!matched)
			unexpected_message_queue.push_front(std::move(message));
	}
	
	return false;
}

bool SimpleMatcher::has_work()
{
	return (posted_request_queue.size() > 0);
}

}

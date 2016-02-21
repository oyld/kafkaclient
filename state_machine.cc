#include <iostream>
#include <unistd.h>

#include "state_machine.h"
#include "request.h"
#include "response.h"

bool StateMachine::run_ = false;

void StateMachine::SignalHandler(int signal)
{
	std::cout << "Interrupt!" << std::endl;
	run_ = false;
}

StateMachine::StateMachine(Network *network, std::map<std::string, Node*> &nodes)
	: nodes_(nodes)
{
	network_ = network;
}

int StateMachine::Init()
{
	signal(SIGINT, StateMachine::SignalHandler);
	return 0;
}

int StateMachine::Start()
{
	event_ = Event::STARTUP;
	current_state_ = &StateMachine::DiscoverCoordinator;
	run_ = true;

	std::cout << "State in Start" << std::endl;

	while (run_)
	{
		(this->*current_state_)(event_);
		sleep(1);
	}

	return 0;
}

int StateMachine::Stop()
{
	// set state
	
	return 0;
}

int StateMachine::DiscoverCoordinator(Event &event)
{
	std::cout << "State in DiscoverCoordinator" << std::endl;

	if (event != Event::STARTUP)
	{
		std::cout << "DiscoverCoordinator state has a invalid indication!" << std::endl;
		return -1;
	}

	GroupCoordinatorRequest *group_request = new GroupCoordinatorRequest(0, "group");

	// Random select a node
	auto it = nodes_.begin();
	std::advance(it, rand() % nodes_.size());
	Node *node = it->second;

	PushRequest(node, group_request);

	Response *response;
	PopResponse(node, &response);

	if (response->api_key_ != ApiKey::GroupCoordinatorRequest)
	{
		return -1;
	}

	GroupCoordinatorResponse *coor_response = dynamic_cast<GroupCoordinatorResponse*>(response);
	coor_response->Print();
	delete coor_response;

	// next state
	current_state_ = &StateMachine::PartOfGroup;
	event = Event::JOIN_REQUEST_WITH_EMPTY_CONSUMER_ID;

	return 0;
}

int StateMachine::PartOfGroup(Event &event)
{
	std::cout << "State in PartOfGroup" << std::endl;

	switch(event)
	{
		case Event::JOIN_REQUEST_WITH_EMPTY_CONSUMER_ID:
		{
			std::vector<std::string> topic({"test"});
			JoinGroupRequest *join_request = new JoinGroupRequest(1, "group", "", topic);
			
			join_request->Print();
			Node *node = nodes_["w-w1902.add.nbt.qihoo.net"];

			PushRequest(node, join_request);

			Response *response;
			PopResponse(node, &response);

			if (response->api_key_ != ApiKey::JoinGroupRequest)
			{
				return -1;
			}

			JoinGroupResponse *join_response = dynamic_cast<JoinGroupResponse*>(response);
			join_response->Print();
			delete join_response;

			MetadataRequest *metadata_request = new MetadataRequest(2, topic);
			metadata_request->Print();
			PushRequest(node, metadata_request);

			// next state
			event = Event::HEARTBEAT;

			break;
		}
		case Event::HEARTBEAT:
		{
			std::cout << "heartbeat..." << std::endl;
			break;
		}
		default:
		{
			break;
		}
	}

	return 0;
}


int StateMachine::PushRequest(Node *node, Request *request)
{
	int fd = node->fd_;
	network_->send_queues_[fd].Push(request);
	return 0;
}

int StateMachine::PopResponse(Node *node, Response **response)
{
	int fd = node->fd_;

	// -1: wait forever
	*response = network_->receive_queues_[fd].Pop(-1);

	return 0;
}

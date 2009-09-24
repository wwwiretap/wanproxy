#include <common/buffer.h>

#include <event/action.h>
#include <event/callback.h>
#include <event/event_system.h>

#include <io/socket.h>

#include <net/tcp_client.h>

TCPClient::TCPClient(SocketAddressFamily family)
: log_("/tcp/client"),
  family_(family),
  socket_(NULL),
  close_action_(NULL),
  connect_action_(NULL),
  connect_callback_(NULL)
{ }

TCPClient::~TCPClient()
{
	ASSERT(connect_action_ == NULL);
	ASSERT(connect_callback_ == NULL);
	ASSERT(close_action_ == NULL);

	if (socket_ != NULL) {
		delete socket_;
		socket_ = NULL;
	}
}

Action *
TCPClient::connect(const std::string& name, EventCallback *ccb)
{
	ASSERT(connect_action_ == NULL);
	ASSERT(connect_callback_ == NULL);
	ASSERT(socket_ == NULL);

	socket_ = Socket::create(family_, SocketTypeStream, "tcp", name);
	if (socket_ == NULL) {
		ccb->event(Event(Event::Error, 0));
		Action *a = EventSystem::instance()->schedule(ccb);

		delete this;

		return (a);
	}

	EventCallback *cb = callback(this, &TCPClient::connect_complete);
	connect_action_ = socket_->connect(name, cb);
	connect_callback_ = ccb;

	return (cancellation(this, &TCPClient::connect_cancel));
}

void
TCPClient::connect_cancel(void)
{
	ASSERT(close_action_ == NULL);
	ASSERT(connect_action_ != NULL);

	connect_action_->cancel();
	connect_action_ = NULL;

	if (connect_callback_ != NULL) {
		delete connect_callback_;
		connect_callback_ = NULL;
	} else {
		/* Caller consumed Socket.  */
		socket_ = NULL;

		delete this;
		return;
	}

	ASSERT(socket_ != NULL);
	EventCallback *cb = callback(this, &TCPClient::close_complete);
	close_action_ = socket_->close(cb);
}

void
TCPClient::connect_complete(Event e)
{
	connect_action_->cancel();
	connect_action_ = NULL;

	e.data_ = (void *)socket_;

	connect_callback_->event(e);
	connect_action_ = EventSystem::instance()->schedule(connect_callback_);
	connect_callback_ = NULL;
}

void
TCPClient::close_complete(Event e)
{
	close_action_->cancel();
	close_action_ = NULL;

	switch (e.type_) {
	case Event::Done:
		break;
	default:
		ERROR(log_) << "Unexpected event: " << e;
		break;
	}

	delete this;
}

Action *
TCPClient::connect(SocketAddressFamily family, const std::string& name, EventCallback *cb)
{
	TCPClient *tcp = new TCPClient(family);
	return (tcp->connect(name, cb));
}

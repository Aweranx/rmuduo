#include <cstdlib>

#include "EPollPoller.h"
#include "Poller.h"

using namespace rmuduo;

Poller* Poller::newDefaultPoller(EventLoop* loop) {
  if (::getenv("RMUDUO_USE_POLL")) {
    return nullptr;
  } else {
    return new EPollPoller(loop);
  }
}
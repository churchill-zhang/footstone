#include "footstone/loop_worker_impl.h"

#include <CoreFoundation/CoreFoundation.h>

namespace footstone {
inline namespace runner {

class LoopWorkerImpl: public Worker {
 public:
  LoopWorkerImpl();
  virtual ~LoopWorkerImpl();

  virtual std::unique_ptr<Task> DoGetNextTask() override;
  virtual void Notify() override;
  virtual void Run() override;
  virtual void WaitFor(const TimeDelta& delta) override;
 private:
  static void OnTimerFire(CFRunLoopTimerRef timer, MainWorker* loop);

  CFRunLoopTimerRef delayed_wake_timer_;
  CFRunLoopRef loop_;
};

}
}
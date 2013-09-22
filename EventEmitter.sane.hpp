#ifndef __EVENTEMITTER_SANE_HPP
#define __EVENTEMITTER_SANE_HPP

#ifdef __EVENTEMITTER_TESTING
#undef __EVENTEMITTER_PROVIDER
#undef __EVENTEMITTER_PROVIDER_THREADED
#undef __EVENTEMITTER_PROVIDER_DEFERRED
#endif

#include <functional>
#include <forward_list>
#include <set>
#include <unordered_set>
#include <memory>
#include <deque>


#include <mutex>
#ifndef EVENTEMITTER_DISABLE_THREADING
#include <condition_variable>
#include <future>
#endif

#define __EVENTEMITTER_CONCAT_IMPL(x, y) x ## y
#define __EVENTEMITTER_CONCAT(x, y) __EVENTEMITTER_CONCAT_IMPL(x, y)

#ifndef __EVENTEMITTER_NONMACRO_DEFS
#define __EVENTEMITTER_NONMACRO_DEFS
namespace EventEmitter {
	
	class DeferredBase {
		// TODO: disable mutex when EVENTEMITTER_DISABLE_THREADING
	protected: 
		typedef std::function<void ()> DeferredHandler;
		std::deque<DeferredHandler>* deferredQueue = nullptr;
		std::mutex mutex;
		inline void assureContainer() {
			if(!deferredQueue)
				deferredQueue = new std::deque<DeferredHandler>();
		}
	protected:
		virtual ~DeferredBase() {
			std::lock_guard<std::mutex> guard(mutex);
			if(deferredQueue)
				delete deferredQueue;
		}
		void runDeferred(DeferredHandler f) {
			std::lock_guard<std::mutex> guard(mutex);
			assureContainer();
			deferredQueue->push_back(f);
		}
	public:
		void clearDeferred() {
			std::lock_guard<std::mutex> guard(mutex);
			assureContainer();
			deferredQueue->clear();
		}
		bool runDeferred() {
			std::lock_guard<std::mutex> guard(mutex);
			assureContainer();
			if(deferredQueue->size()) {
				((deferredQueue->front()))();
				deferredQueue->pop_front();
			}
			return deferredQueue->size();
		}
		void runAllDeferred() {
			while(runDeferred());
		}
	};
	
	// reference_wrapper needs to be use instead of std::reference_wrapper
	// this is because of VS2013 (RC) bug
	template<class T> class reference_wrapper
	{
	public:
		typedef T type;
		explicit reference_wrapper(T& t): t_(&t) {}
		operator T& () const { return *t_; }
		T& get() const { return *t_; }
		T* get_pointer() const { return t_; }
private:
    T* t_;
};
	

  // source: stackoverflow.com/questions/15501301/binding-function-arguments-in-c11
	template<typename T> struct forward_as_ref_type {
   typedef T &&type;
	};
	template<typename T> struct forward_as_ref_type<T &> {
		typedef reference_wrapper<T> type;
	};
  template<typename T> typename forward_as_ref_type<T>::type forward_as_ref(
   typename std::remove_reference<T>::type &t) {
      return static_cast<typename forward_as_ref_type<T>::type>(t);
   }
   template<typename T> T &&forward_as_ref(
   typename std::remove_reference<T>::type &&t) {
      return t;
	 }
 // 	template<class T>
 //   std::reference_wrapper<T> maybe_ref(T& v, int){ return std::ref(v); }
 //   template<class T> T&& maybe_ref(T&& v, long){ return std::forward<T>(v); }
 // ^^^^^^ alternative way, possibly more hackish
	
	template<typename... Args>
	class LambdaCallbackWrapper
	{
		std::function<void(Args...)> m_f;
		std::function<void()> m_afterCb;
	public:
		LambdaCallbackWrapper(const std::function<void(Args...)>& f, const std::function<void()>& afterCb) : m_f(f), m_afterCb(afterCb) {}
		void operator()(Args... fargs) const { m_f(fargs...); m_afterCb();}
	};
	template<typename... Args>
	LambdaCallbackWrapper<Args...> wrapLambdaWithCallback(const std::function<void(Args...)>& f, const std::function<void()>& afterCb) {
		return LambdaCallbackWrapper<Args...>(f, afterCb);
	};
	template<typename... Args>
	class LambdaBeforeCallbackWrapper
	{
		std::function<void(Args...)> m_f;
		std::function<void()> m_beforeCb;
	public:
		LambdaBeforeCallbackWrapper(const std::function<void(Args...)>& f, const std::function<void()>& beforeCb) : m_f(f), m_beforeCb(beforeCb) {}
		void operator()(Args... fargs) const { m_beforeCb(); m_f(fargs...);}
	};
	template<typename... Args>
	LambdaBeforeCallbackWrapper<Args...> wrapLambdaWithBeforeCallback(const std::function<void()>& beforeCb, const std::function<void(Args...)>& f) {
		return LambdaCallbackWrapper<Args...>(f, beforeCb);
	};

#ifndef EVENTEMITTER_DISABLE_THREADING
	
	// TODO: allow callback for setting if async has completed
	template<typename... Args>
	class LambdaAsyncWrapper
	{
		std::function<void(Args...)> m_f;		
	public:
		LambdaAsyncWrapper(const std::function<void(Args...)>& f) : m_f(f) {}
		void operator()(Args... fargs) const { 
			std::async(std::launch::async, m_f, fargs...);
		}
	};
	template<typename... Args>
	LambdaAsyncWrapper<Args...> wrapLambdaInAsync(const std::function<void(Args...)>& f) {
		return LambdaAsyncWrapper<Args...>(f);
	};
	
	template<typename... Args>
	class LambdaPromiseWrapper
	{
		std::shared_ptr<std::promise<std::tuple<Args...>>> m_promise;
	public:
		LambdaPromiseWrapper(std::shared_ptr<std::promise<std::tuple<Args...>>> promise) : m_promise(promise) {}
		void operator()(Args... fargs) const { 
			m_promise->set_value(std::tuple<Args...>(fargs...));
		}
	};
	template<typename... Args>
	LambdaPromiseWrapper<Args...> getLambdaForFuture(std::shared_ptr<std::promise<std::tuple<Args...>>> promise) {
		return LambdaPromiseWrapper<Args...>(promise);
	};

#endif // EVENTEMITTER_DISABLE_THREADING
	
}
#endif // __EVENTEMITTER_NONMACRO_DEFS


#ifndef __EVENTEMITTER_CONTAINER
#define __EVENTEMITTER_CONTAINER std::forward_list<HandlerPtr>
#endif

#define __EVENTEMITTER_PROVIDER(frontname, name) //^//
template<typename... Rest>
class ExampleEventProvider {
public:
	typedef std::function<void(Rest...)> Handler;
	typedef std::shared_ptr<Handler> HandlerPtr;
private:
 	struct EventHandlersSet : public __EVENTEMITTER_CONTAINER {
		EventHandlersSet() : eraseLast(false) {}
 		bool eraseLast : 8;
 	};
	EventHandlersSet* eventHandlers = nullptr;
public:
	HandlerPtr onExample (Handler handler) {
	  if(!eventHandlers)
			eventHandlers = new EventHandlersSet;
		eventHandlers->emplace_front(std::make_shared<Handler>(handler));
		return eventHandlers->front();
	}
	HandlerPtr onceExample (Handler handler) {
	  if(!eventHandlers)
			eventHandlers = new EventHandlersSet;
		eventHandlers->emplace_front( std::make_shared<Handler>(EventEmitter::wrapLambdaWithCallback(handler, [=]() {
			eventHandlers->eraseLast = true;
		})));
		return eventHandlers->front();
	}
	template<typename... Args> inline void triggerExample (Args&&... fargs) {
	  if(!eventHandlers)
			return;
		auto prev = eventHandlers->before_begin(); 
	  for(auto i = eventHandlers->begin();i != eventHandlers->end();) {
			
			(*(*i))(fargs...);
			if(eventHandlers->eraseLast) {
				i = eventHandlers->erase_after(prev);
				eventHandlers->eraseLast = false;
			}
			else {
				++i;
				++prev;
			}
		}
	}
	HandlerPtr removeExampleHandler (HandlerPtr handlerPtr) {
 	  if(eventHandlers) { 			
 			auto prev = eventHandlers->before_begin(); 
			for(auto i = eventHandlers->begin();i != eventHandlers->end();++i,++prev) { 
				if((i)->get() == handlerPtr.get()) {
					eventHandlers->erase_after(prev);
					return handlerPtr;
				}
			}
 		}
 		return HandlerPtr();
	}
	void removeAllExampleHandlers () {
		if(eventHandlers)
			eventHandlers->clear();
	}
	~ExampleEventProvider() {
		if(eventHandlers) {
			delete eventHandlers;
		}
	}
}; //_//

#define __EVENTEMITTER_PROVIDER_DEFERRED(frontname, name) //^//
template<typename... Rest>
class ExampleDeferredEventProvider : public ExampleEventProvider<Rest...>, public virtual EventEmitter::DeferredBase {
public:
	template<typename... Args> void triggerExample (Args&&... fargs) {
		runDeferred(
			std::bind([=](Args... as) {
			this->ExampleEventProvider<Rest...>::triggerExample(as...);
			}, EventEmitter::forward_as_ref<Args>(fargs)...));
	}
}; //_//

#ifndef EVENTEMITTER_DISABLE_THREADING

#define __EVENTEMITTER_PROVIDER_THREADED(frontname, name) //^//
template<typename... Rest>
class ExampleThreadedEventProvider : public ExampleEventProvider<Rest...>, public virtual EventEmitter::DeferredBase { 
  std::condition_variable condition;
	std::mutex m;

	typedef typename ExampleEventProvider<Rest...>::Handler Handler;
	typedef typename ExampleEventProvider<Rest...>::HandlerPtr HandlerPtr;
	
public:
	ExampleThreadedEventProvider() {
	}	
	bool waitExample (std::chrono::milliseconds duration = std::chrono::milliseconds::max()) {
		waitExample([=](Rest...) {
		}, duration);
 	}
 	bool waitExample (Handler handler, std::chrono::milliseconds duration = std::chrono::milliseconds::max()) {
		std::shared_ptr<std::atomic<bool>> finished = std::make_shared<std::atomic<bool>>();
		std::unique_lock<std::mutex> lk(m);
		HandlerPtr ptr = onceExample(
			EventEmitter::wrapLambdaWithCallback(handler, [=]() {
				finished->store(true);
				this->condition.notify_all();
		}));
		
		if(duration == std::chrono::milliseconds::max()) {
			condition.wait(lk);
		} 
		else {
			condition.wait_for(lk, duration, [=]() {
				return finished->load();
			});
		}
		bool gotFinished = finished->load();
		if(!gotFinished) {
			ExampleEventProvider<Rest...>::removeExampleHandler(ptr);
		}
		return gotFinished;
 	}
	void asyncWaitExample(Handler handler, std::chrono::milliseconds duration, const std::function<void()>& asyncTimeout) {
		auto async = std::async(std::launch::async, [=]() {
			if(!waitExample(handler, duration))
				asyncTimeout();
		});
	}
	
	HandlerPtr onExample (Handler handler) {
		std::lock_guard<std::mutex> guard(mutex);
		return ExampleEventProvider<Rest...>::onExample(handler);
	}
	HandlerPtr onceExample (Handler handler) {
		std::lock_guard<std::mutex> guard(mutex);
		return ExampleEventProvider<Rest...>::onceExample(handler);
	}
	HandlerPtr asyncOnExample (Handler handler) {
		return onExample(EventEmitter::wrapLambdaInAsync(handler));
	}
	HandlerPtr asyncOnceExample (Handler handler) {
		return onceExample(EventEmitter::wrapLambdaInAsync(handler));
	}
	auto futureOnceExample() -> decltype(std::future<std::tuple<Rest...>>()) {
		typedef std::tuple<Rest...> TupleEventType;
		auto promise = std::make_shared<std::promise<TupleEventType>>();
		auto future = promise->get_future();
		condition.notify_all();
		onceExample(EventEmitter::getLambdaForFuture(promise));
		return future;
	}
	template<typename... Args> void triggerExample (Args&&... fargs) { 
		std::lock_guard<std::mutex> guard(mutex);
		ExampleEventProvider<Rest...>::triggerExample(fargs...);
		condition.notify_all();
	}
	template<typename... Args> void deferExample (Args&&... fargs) { 
		runDeferred(
 			std::bind([=](Args... as) {
 			// NOTE: with this it does not fail!!! without this it fails
 			this->ExampleEventProvider<Rest...>::triggerExample(as...);
 			},
			EventEmitter::forward_as_ref<Args>(fargs)...			
			//fargs...
			));
	}
}; //_//

#endif // EVENTEMITTER_DISABLE_THREADING

#include <map>

#define EventDispatcherBase ExampleEventProvider //#//

 #define __EVENTEMITTER_DISPATCHER(frontname, name) //^//
template<template<typename...> class EventDispatcherBase, typename T, typename... Rest>
class ExampleEventDispatcher : public EventDispatcherBase<T, Rest...> {
 	typedef typename EventDispatcherBase<Rest...>::HandlerPtr HandlerPtr;
	typedef typename EventDispatcherBase<Rest...>::Handler Handler;
	std::multimap<T, HandlerPtr> map;
	bool eraseLast = false;
public:
	ExampleEventDispatcher() {
		ExampleEventProvider<T, Rest...>::onExample([&](T first, Rest... fargs) {
 			auto ret = map.equal_range(first);
 			for(auto it = ret.first;it != ret.second;) {
 				(*(it->second))(fargs...);
				if(eraseLast) {
					map.erase(it);
					eraseLast = false;
				}
				else {
					++it;
				}
 			}
		});
	}
 	HandlerPtr onExample (T first, Handler handler) {
		return map.insert(std::pair<T, HandlerPtr>(first,  std::make_shared<Handler>(handler)))->second;
 	}
 	HandlerPtr onceExample (T first, Handler handler) {
		return map.insert(std::pair<T, HandlerPtr>(first,  std::make_shared<Handler>(
			wrapLambdaWithCallback(handler, [&] {
				eraseLast = true;
			}))))->second;
 	}
 	HandlerPtr removeExampleHandler (T first, Handler handler) {
		// TODO
	}
	HandlerPtr removeAllExampleHandlers (T first, Handler handler) {
		// TODO
	}
 }; //_//


#if 0 //#//

#define DefineEventProvider(name, ...) \
__EVENTEMITTER_PROVIDER(__EVENTEMITTER_CONCAT(Tpl, name), name) \
typedef __EVENTEMITTER_CONCAT(__EVENTEMITTER_CONCAT(Tpl, name), EventProvider) <__VA_ARGS__> __EVENTEMITTER_CONCAT(name, EventProvider);

#define DefineEventProviderDeferred(name, ...) \
__EVENTEMITTER_PROVIDER(__EVENTEMITTER_CONCAT(Tpl, name),name) \
__EVENTEMITTER_PROVIDER_DEFERRED(__EVENTEMITTER_CONCAT(Tpl, name),name) \
typedef __EVENTEMITTER_CONCAT(__EVENTEMITTER_CONCAT(Tpl, name), DeferredEventProvider)  <__VA_ARGS__> __EVENTEMITTER_CONCAT(name, DeferredEventProvider);

#define DefineEventProviderThreaded(name, ...) \
__EVENTEMITTER_PROVIDER(__EVENTEMITTER_CONCAT(Tpl, name),name) \
__EVENTEMITTER_PROVIDER_THREADED(__EVENTEMITTER_CONCAT(Tpl, name),name) \
typedef __EVENTEMITTER_CONCAT(__EVENTEMITTER_CONCAT(Tpl, name), ThreadedEventProvider) <__VA_ARGS__> __EVENTEMITTER_CONCAT(name, ThreadedEventProvider);

#endif //#//

__EVENTEMITTER_PROVIDER(/**/,/**/)
__EVENTEMITTER_PROVIDER_DEFERRED(/**/,/**/)

#ifndef EVENTEMITTER_DISABLE_THREADING
__EVENTEMITTER_PROVIDER_THREADED(/**/,/**/)
#endif

#endif // __EVENTEMITTER_SANE_HPP
# event

```c++
struct event {
  //和event相关联的回调函数
	struct event_callback ev_evcallback; 

	/* 用于管理超时 */
	union {
		TAILQ_ENTRY(event) ev_next_with_common_timeout;
		size_t min_heap_idx;
	} ev_timeout_pos;
  //用于保存文件描述符
	evutil_socket_t ev_fd;

  //记录事件 
	short ev_events;
	short ev_res;		/* result passed to event callback */

  //相关联的event_base
	struct event_base *ev_base;

	union {
    //IO events
		/* used for io events */
		struct {
			LIST_ENTRY (event) ev_io_next;
			struct timeval ev_timeout;
		} ev_io;

    //信号 events
		/* used by signal events */
		struct {
			LIST_ENTRY (event) ev_signal_next;
			short ev_ncalls;
			/* Allows deletes in callback */
			short *ev_pncalls;
		} ev_signal;
	} ev_;


	struct timeval ev_timeout;
};
```

1. `ev_evcallback`

   ```c++
   struct event_callback {
     //event_base->activequeues 记录着激活的event_callback
   	TAILQ_ENTRY(event_callback) evcb_active_next;
   	short evcb_flags;
   	ev_uint8_t evcb_pri;	/* smaller numbers are higher priority */
   	ev_uint8_t evcb_closure;
     union {
   		void (*evcb_callback)(evutil_socket_t, short, void *);
   		void (*evcb_selfcb)(struct event_callback *, void *);
   		void (*evcb_evfinalize)(struct event *, void *);
   		void (*evcb_cbfinalize)(struct event_callback *, void *);
   	} evcb_cb_union;
   	void *evcb_arg;
   };
   ```

2. `ev_timeout_pos`

   用于超时管理。

   ```c++
   union {
   		TAILQ_ENTRY(event) ev_next_with_common_timeout;
   		size_t min_heap_idx;
   	} ev_timeout_pos;
   ```

3. `ev_`

   ```c++
   union {
   		TAILQ_ENTRY(event) ev_next_with_common_timeout;
   		size_t min_heap_idx;
   	} ev_timeout_pos;
   ```




EXPERIMENTAL! NOT YET PRODUCTION READY!

# HiTime (Hierachical Timeout Manager)
This is an implementation of a hierarchical timeout manager.
Note that this library was renamed hitime, from hightime.

Of note, this structure can also be used as a starvation-free priority queue.
Please see the appropriate section below.


**Disclaimer:**
Use at your own risk.
I've searched for another implementation that uses the ideas presented here.
I still haven't found anything at the time of this writing (or subsequent updates).
This manager is hierarchical and operates on powers of two.


## Contents
- [Brief History](#brief-history)
- [Code Examples](#code-examples)
- [Build](#build)
- [Testing](#testing)
- [Performance Notes](#perf-notes)
- [Time Complexity](#time-complexity)
- [Space Complexity](#space-complexity)
- [Coding Standards](#coding-standards)
- [Terms](#terms)
- [Features](#features)
- [Design](#design)
- [End-of-Time](#end-of-time)
- [Compatibility](#compatibility)
- [Demonstration](#demonstration)
- [Starvation-Free Priority Queue](#priority-q)
- [Reading Materials](#reading-materials)
- [TODO](#todo)


## Brief History | Background | Monologue
<a name="brief-history" />

One day I, the author, needed a timeout manager.
I read [a paper](#wheel-paper) on different timeout manager designs and I didn't like any of the designs.
I then dreamt up my own solution.
Really, I just realized that monotonically increasing time is just flipping bits progressively at fixed intervals.
So integral values are perfect for representing time.
Time also plays nicely with the exponential meaning of the bits of an integral value.
So I created a structure where each bucket represents the bits of a timeout value.
So when time progresses and the bits get flipped in your timeout, then that bucket gets processed.
I prefer this to "per tick" type timeouts because it seems so wasteful to have to check for timeouts on every tick.
My method is logarithmic in the timedelta, but I think that every timeout manager has to expend similar costs.
Plus, there are tricks to reduce the number of times a timeout moves buckets.
There are some oddities that arose in the original attempt at implementing this, which led to a redesign of the algorithm.
Overall, I believe that my intuition is correct and I'm pleased with the general design.


## Code Examples
<a name="code-examples" />

In the following examples the chosen granularity is milliseconds (ms).
Note that you get to choose the granularity since the framework is agnostic.
It is recommended to wrap the functions so you don't make a mistake.

1. Include:

        #include "hitime.h"

1. Create:

        hitime_t ht;
        hitime_init(&ht);

1. Start:

        uint64_t interval = 5*60*1000; // 5 minutes
        void *data = NULL; // Your data
        hitimeout_t *t = hitimeout_new(); // hitimeout_init for embedded structs
        hitimeout_set(t, hitime_now_ms() + interval, data);
        hitime_start(&ht, t);

1. Start within a range:

        uint64_t now = hitimeout_now_ms();
        hitime_start_range(&ht, t, now + 32, now + 64);

1. Update the time of an active or inactive timeout:

        hitime_touch(&ht, t, now + 10);

1. Stop:

        // Safe to call if you didn't start if you used hitimeout_init/new
        hitime_stop(&ht, t);

1. Timeout:

        sleep_ms(hitime_get_wait(&ht));
        if (hitime_timeout(&ht, hitimeout_now_ms()))
        {
            hitimeout_t *t;
            while ((t = hitime_get_next(&ht)))
            {
                void *data = hitimeout_data(t);
                // Process your data
            }
        }

1. Destroy:

        hitimeout_t *t;
        hitime_expire_all(&ht);
        while ((t = hitime_get_next(&ht)))
        {
            hitimeout_free(&t);
        }
        hitime_destroy(&ht);


## Build
<a name="build" />

Use the meson build system.

```bash
# Build
meson setup build && cd build/ && meson compile

TODO add code coverage generation
```


## Testing
<a name="testing" />

Run `./build/prove` after building to run the tests.


## Performance Notes
<a name="perf-notes" />

While doing some primitive bench-marking I got an increase in performance
of 40% between the `perform.c` benchmark and the cache-friendly `cache.c` benchmark.
Your mileage will vary, but just a ballpark figure for you.

Linking against a static build of this library will further increase performance.


## Time Complexity
<a name="time-complexity" />

The following will need to be vetted, but...
* O(1) for `hitime_start` per call
* O(1) for `hitime_stop` per call
* O(1) for `hitime_get_wait` per call
* O(1) for `hitime_get_next` per call
* O(n\*b) for `hitime_timeout`
  This is the worst case for the entire data structure, not a single operation.
  Note that we could cheat and say O(n) because the second term
  deals with the number of bits/bins, which is constant
  ([see design](#design)).
  This is approximately O(n\*log\_n) for sufficiently large `n`.
* O(b) for `hitime_expire_all` (per call), where `b` is the number of bits/bins.
  Again we could cheat and claim O(1) since `b` is constant.


## Space Complexity
<a name="space-complexity" />

Each struct is fixed and space complexity only grows linearly with the number of timeouts.
Each `hitimeout_t` is roughly 36 octets on a 64-bit system.
The `hitime_t` struct is about 544 octets (33\*2\*8 + 8 + 2\*4) on a 64-bit system.
Needless to say this does not nicely fit on a 64 octet cache line.


## Coding Standards
<a name="coding-standards" />

I loosely follow the Barr Group coding standards.
I find their standards a good basis for C programming.


## Terms
<a name="terms" />

- **time:** The timeout manager's concept of time including units and granularity.
- **now:** The timeout manager's idea of the current time.
- **timeout:** The timestamp in the future where we want to trigger an event.
- **timedelta:** The difference between **now** and **timeout**.


## Features
<a name="features" />

The following are reasons you might want to use this timeout manager:
1. Constant time start and stop.
   Great if many timeouts will be stopped before expiration.
   In fact, the timeout will only be touched a maximum of two times, and likely
   only once, before half of the timeout period has elapsed.
   This only helps prevent performing operations in use-cases that timeouts
   are likely to expire.
1. Bulk timeouts.
   Great if running on a single multiplexing thread that occassionally is busy.
   So if 8ms elapses, all timeouts in bins 4ms and lower are bulk expired.
1. Timeouts happen in batched operations.
   This may give a performance boost depending on the locality and allocation
   choices used with the timeout structs.
1. Granularity agnostic.
   This can be a pitfall, but can be used for milliseconds, seconds, or
   any other granularity.
   Be warned that below milliseconds is not recommended;
   the code can be retooled to better support sub-milliseconds.
1. 100% core code coverage (`hitime.c`).


## Design
<a name="design" />

This tool is designed to make no assumptions as to the granularity of time you want to use.
As long as you are consistent, the framework doesn't care.

This framework does require the following assumptions:
1. Time is monotonically increasing.
1. All time is of the same granularity (you must make any conversions).

To avoid expensive timeouts that consume memory and CPU resources, make sure that
timeouts are not greater than (2^(bits-1)) - 1 from the current time.
Note that I am using 32 bits/bins in the internal implementation.
Again, this can be violated, but at a performance cost.
In milliseconds, this means don't have timeouts more than about 24 days out.
At that point I would use a database to prevent memory bloat, unless there are only a few timeouts that far out.

The peculiar design of this tool is so it could be tested quickly and reliably.
This is why the user must pass in the current time.
The user may call `hitime_timeout` at any time to force an update; although, this function may take some time to execute.
There is a re-entrant version to process timeouts incrementally.


## End-of-Time
<a name="end-of-time" />

This library deals in powers of 2, time differentials, and bit triggers.
Therefore, wrapping time around on itself will result in predictable and correct behavior.
Of course, further demonstrations will be needed.


## Compatibility
<a name="compatibility" />

I designed this for x86\_64 arch, but any 64 bit will likely work.
I test all of my code on Linux.


## Demonstration
<a name="demonstration" />

Proving that this method works is critical.
I may not be able to prove it satifactorily enough, but here is at least a discussion.

The design is to have 32 bins.
Each bin corresponds to bits in a 32 bit number of the bitwise difference between `now` and the timeout `t` (xor).
Thus, the timeout is placed in the bin of the order of the xor.
This makes the manager a hierarchical timeout manager.
The timeout will be assigned to bins the same number of times as `order(t^n)`.
This gives a logarithmic-type complexity based on the timeout;
shorter timeouts will have fewer reassignments, longer timeouts will have more.

The idea is that once a certain amount of time elapses, bins get triggered.
When a bin is triggered the timeouts get reassigned to lower bins until expiry.
Bins get triggered when the bin's corresponding bit in the timeout gets flipped.
Alternatively if a bit of a bucket larger is flipped, all the lesser buckets get visited.

The reason for the (2^(bits-1)) - 1 limit on the timeout is to better handle overflow.
The addition of any two numbers cannot exceed the maximum degree of the
operands by more than one.
For example: `111 + 111 = 1110`.
Note that the degree of both operands is 2 (zero indexed), the degree of the answer is 3.
The terms degree, order, and most-significant bit are used interchangeably as they represent the same concept in this context.

The order of timeouts in the expiry is not guaranteed to be sorted from smallest timeout to greatest.
However, the order should be sufficient for most applications.
The order should be FIFO and sorted iff the wait times are strictly adhered to and timeouts are greater than now when added.
This fact will need to be proved more rigorously to be relied upon.


#### Naive Example
<a name="naive-example" />

To demonstrate the core idea, but know that this is a naive example that omits many factors and makes certain assumptions.
I got the idea for this design by the following reasoning and then reworking
the reasoning.

Imagine a timeout, a state in the future.
If the current time is zero, the start of the epoch, then the timeout is the
time from now to when the timeout should be evaluated.
Thus the timeout is the delta or difference between now and timeout.
The timeout can be viewed as a sequence of set bits.
For example: `0100 1101`.

If we were to wait `0100 0000` units of time then the delta would become `0000 1101`.
Thus we have removed the largest order from the bit sequence and have become closer to timeout.
Next we would need to wait `0000 1000` to remove the next largest bit.
This would continue until the current time is the timeout time and thus we expire the timeout.

This illustrates the idea that we place a timeout into a bin corresponding to the topmost bit.
Then once the amount of time the bin represents has transpired we re-evaluate the timeout and place it into a lower bin.
This process repeats until the timeout experiences expiry.


#### Start Algorithm
<a name="algorithm-start" />

Each timeout has a static timestamp `t` indicating when in the future it expires.
From the static timestamp and now timestamp we can compute the difference.
Note that if `t` is expired we have achieved the goal and further calculations are invalid.
If the timeout is outside the range tracked by the bins [0, 30], then the timeout is placed in the overflow bin [31].
Placing a timeout in the overflow bin is inefficient.
If the timeout is far outside the tracked time range, it becomes expensive from a Big-Oh perspective.
Now that we've discussed edge cases, the normal case is the timeout is in the normal range and is disscussed in `ENQUEUE`.

    START(t):
    if t in list: return
    if t is expired:
        add t to expiry list
    otherwise:
        ENQUEUE(t)
    return


#### Enqueue Algorithm
<a name="algorithm-enqueue" />

Timeout `t` is placed in the bin defined by `order(t^n)` which is the same
as finding the index of the most-significant bit.
Note that `order(t^n) >= order(delta)`; this fact will be proved [here](#prove-order).

    ENQUEUE(t):
    delta = time(t) - now
    if delta too large:
        enqueue t in overflow bin
    otherwise:
        index = get index of highest differing bit of timeout and now
        set bit in bitset according to index
        set timeout index
        enqueue t in bin given by index


#### Stop Algorithm
<a name="algorithm-stop" />

Stopping a timeout is trivial and only requires removing it from a list
and clearing the bitset indicating which bins are set if that list is empty.

    STOP(t):
    if t in list:
        unlink t from list
        check if list is empty
        clear bitset of bins if empty


#### Wait Algorithm
<a name="algorithm-wait" />

    WAIT(now, t):
    last = previous now
    diff = now - last
    tdiff = t - last
    if diff < tdiff
        return tdiff - diff
    otherwise:
        return 0


#### Timeout Algorithm
<a name="algorithm-timeout" />

To fully prove that this scheme works we must demonstrate that
`order(t_1^n_1) < order(t_0^n_0)`.
This will guarantee that each time a timeout is inspected the bin the timeout
is reassigned to will be less than the bin it is currently placed into.
This works because of the relationship between the order and the bin.
Obviously the overflow bin is an exception to this rule.

    TIMEOUT(now):
    last = previous now
    check that now > last
    expire every timeout in bin 0
    lapsed = the amount of time that elapsed or max amount if too great
    lapsed_index = get index of highest bit of lapsed
    expire all bins in range [1, lapsed_index)
    check_index = get index of highest bit of (now ^ last)
    check all timeouts in bins in range [lapsed_index, check_index]
        if timeout is expired: add to expiry
        otherwise: ENQUEUE(t)
    if overflow bin is triggered:
        check all timeouts in overflow, but move list to prevent endless cycle


#### Prove Order
<a name="prove-order" />

Prove that `order(t^n) >= order(delta)`.
Temporarily this fact is intuitively derived.

    Let delta = t - n.

    Since delta + n, which is t, causes the bits to potentially cascade,
    then excluding the same terms between t and n results in an order greater
    than or equal to delta.


## Starvation-Free Priority Queue
<a name="priority-q" />

Instead of the tick items being time, they now represent priority increments.
Instead of using the time the timeout is to expire, you pass the priority relative to the priority counter (now).
Something with immediate priority would be less-than or equal-to the current time/priority counter.

If a user has two priorities A and B, where A has a high priority of one (1) and B a lower priority of four (4)
then you add them to the queue where the "time" of A is now + 1, and B is now + 4.
Then you increment the priority counter (now).
Run anything that was "expired".
Then either you can do something else, or increment the counter, OR if you want to jump to the next priority add to now the wait time.
Rinse and repeat.
Lower priorities bubble up the queue and naturally get intermixed with higher priority items.
Higher priority items will naturally be placed at the top of the queue.

WARNING: There is an edge case where using the technique of immediate priority will place an item on the expiry queue.
This does NOT place it ahead of lower-priority items on the expiry queue.
The code would need to be modified for such behavior.

WARNING: There is another edge case where using the wait time on an empty queue may just cause you to spin your wheels in effectively an infinite loop.

The time and space complexity would be about the same as a binary heap (which would probably have better performance, depending on your use-case).
The only advantages that you might have with this approach is that items can keep track of their own priority.
**AND**
If lower priority items have a high chance of being cleared (removed from needing to be processed), then you can remove the need to update them in the queue.
**AND**
You can do fine-grained or bucketed priorities using this data-structure (fine-grained is using any number, bucketed is using a range with exponential numbers (1, 2, 4, 8, 16) to guarantee they are placed in the correct bucket (or you could alter this library and its structure).


## Reading Materials
<a name="reading-materials" />

<a name="wheel-paper" />

George Varghese and Anthony Lack (1997).
Hashed and Hierarchical Timing Wheels: Efficient Data Structures for Implementing a Timer Facility.
(IEEE/ACM Transactions on Networking, Vol. 5, No.6, December 1997).


## TODO
<a name="todo" />

- [ ] Search for any similarly designed libraries/tools.
- [ ] A function that allows the user to guarantee the next timeout interval.
      Then incoming timeouts can be placed into more optimal bins.
      I wish I recorded more of my thoughts here so I could remember what I was thinking about...
      Oh, telling the manager when the next timeout occurs will allow you to use a lower bin and fewer tests to move timeouts up the queue.
      You could subtract the difference of the incoming timeouts, then place them according to a different "now".
- [ ] More rigorous demonstrations for the core ideas and any associated testing
- [ ] Test behavior at the end-of-time.
- [ ] Add method to timeout an entire bin.
- [ ] Add method to count an entire bin, good for priority usages and testing.


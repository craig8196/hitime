
# HiTime (Hierachical Timeout Manager)
This is an implementation of a hierarchical timeout manager.
Note that this library was renamed hitime, from hightime.


## Contents
- [Disclaimer](#disclaimer)
- [Features](#features)
- [Design](#design)
- [Misc Notes](#misc-notes)
- [Coding Standards](#coding-standards)
- [Time Complexity](#time-complexity)
- [Space Complexity](#space-complexity)
- [Code Examples](#code-examples)
- [Build](#build)
- [Testing](#testing)
- [Compatibility](#compatibility)
- [Demonstration](#demonstration)
- [Reading Materials](#reading-materials)
- [TODO](#todo)


## Note to the Reader | Disclaimer
<a name="disclaimer" />

Use at your own risk.
I've searched for another implementation that uses the ideas presented here;
I still haven't found anything at the time of this writing.
I read [a paper](#wheel-paper) on different timeout manager designs and
decided I didn't like any of them and would design my own.
This manager is hierarchical and operates on powers of two.
It would not surprise me if this design has already been thought of,
however, this is my implementation and it will serve nicely as an exercise.


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

This tool is designed to make no assumptions as to the granularity of time
you want to use.
As long as you are consistent, the framework doesn't care.

This framework does require the following assumptions:
1. Time is monotonically increasing.
1. Timeouts are not greater than (2^(bits-1)) - 1 from the current time.
   Note that I am using 32 bits/bins in the internal implementation.
   Note that this can be violated, but at a potential performance cost.
   In milliseconds, this means don't have timeouts more than about 10 days out
   (if I recall correctly).
1. All time is of the same granularity (you must make any conversions).

The peculiar design of this tool is so it could be tested quickly and reliably.
This is why the user must pass in the current time.
The user may call `hitime_timeout` at any time to force an update;
although, this function may take some time to execute.


## Misc Notes
<a name="misc-notes" />

While doing some primitive bench-marking I got an increase in performance
of 40% between the `perform.c` benchmark
and the cache-friendly `cache-perf.c` benchmark.
Your mileage will vary, but just a ballpark figure for you.


## Coding Standards
<a name="coding-standards" />

I loosely follow the Barr Group coding standards.
I find their standards a good basis for C programming.


## Time Complexity
<a name="time-complexity" />

The following will need to be vetted, but...
* O(1) for `hitime_start`
* O(1) for `hitime_stop`
* O(1) for `hitime_get_wait`
* O(1) for `hitime_get_next`
* O(n\*b) for `hitime_timeout`
  Note that we could cheat and say O(n) because the second term
  deals with the number of bits/bins, which is constant
  ([see design](#design)).
  This is approximately O(n\*log_n) for sufficiently large `n`.
* O(b) for `hitime_expire_all`, where `b` is the number of bits/bins.
  Again we could cheat and claim O(1) since `b` is constant.


## Space Complexity
<a name="space-complexity" />

Each struct is fixed and space complexity only grows linearly with the 
number of timeouts.
Each `hitimeout_t` is roughly 40 octets on a 64-bit system.
The `hitime_t` struct is about 544 octets (33*2*8 + 8 + 2*4) on a 64-bit system.
Needless to say this does not nicely fit on a 64 octet cache line.


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

        hitimeout_t *t = hitimeout_new();
        hitimeout_set(t, hitime_now_ms(), NULL, 0);
        hitime_start(&ht, t);

1. Stop:

        hitime_stop(&ht, t);

1. Timeout:

        int wait = hitime_get_wait(&ht);
        sleep_ms(wait);
        if (hitime_timeout(&ht, hitimeout_now_ms()))
        {
            hitimeout_t *t;
            while ((t = hitime_get_next(&ht)))
            {
                // process t
            }
        }

1. Destroy:

        hitimeout_t *t;
        hitime_expire_all(&ht);
        while ((t = hitime_get_next(&ht)))
        {
            // process t
        }
        hitime_destroy(&ht);


## Build
<a name="build" />

For those unfamiliar with cmake:

        mkdir build && cd build
        cmake ..
        cmake --build .

For code coverage:

        cmake -DCODE_COVERAGE=ON ..
        cmake --build .

For installation:

        cd build
        sudo cmake --install .
        sudo ldconfig

Cleanup:

        rm -rf build


## Testing
<a name="testing" />

Build tests like you build the project.
Run tests with:

        ctest -VV

Download git submodules and utilities prior to code coverage or doxygen:

        git submodule update --init
        apt install doxygen graphviz lcov

Run code coverage:

        make ccov-prove


## Compatibility Warning
<a name="compatibility" />

I designed this for x86_64 arch.
I test all of my code on Linux.


## Demonstration and Discussion of Concepts
<a name="demonstration" />

Proving that this method works is critical.
I may not be able to prove it satifactorily enough, but here is at least
a discussion.

The design is to have 32 bins, each bin corresponds to bits in a 32 bit
number of the bitwise difference between `now` and the timeout `t` (xor).
Thus, the timeout is placed in the bin of the order of the xor.
This makes the manager a hierarchical timeout manager and should have some
nice properties as previously discussed.
The timeout will be assigned to bins the same number of times as `order(t^n)`.
This gives a logarithmic-type complexity based on the timeout;
shorter timeouts will have fewer reassignments, longer timeouts will have more.

The idea is that once a certain amount of time elapses, bins get triggered.
When a bin is triggered the timeouts get reassigned to lower bins until expiry.

The reason for the (2^(bits-1)) - 1 limit on the timeout is to better
handle overflow.
The addition of any two numbers cannot exceed the maximum degree of the
operands by more than one.
For example: 111 + 111 = 1110.
Note that the degree of both operands is 2 (zero indexed), the degree of the answer is 3.
Also, the terms degree, order, and most-significant bit are used fairly
interchangeably as they represent the same concept in this context.

The order of timeouts in the expiry is not guaranteed to be sorted from
smallest timeout to greatest.
However, the order should be sufficient for most applications.
The order should be FIFO and sorted iff the wait times are strictly adhered to;
granted this fact may need to be proved more rigorously to be relied upon.
An edge-case, or requirement, is that timeouts must be ahead of the 
manager's idea of now; failure to meet this means that timeouts are just FIFO
for the one's added that are already expired.


### Naive Example
<a name="naive-example" />

To demonstrate the core idea, but know that this is a naive example that
omits many factors and makes certain assumptions.
I got the idea for this design by the following reasoning and then reworking
the reasoning.

Imagine a timeout, a state in the future.
If the current time is zero, the start of the epoch, then the timeout is the
time from now to when the timeout should be evaluated.
Thus the timeout is the delta or difference between now and timeout.
The timeout can be viewed as a sequence of set bits.
For example: `0100 1101`.

If we were to wait `0100 0000` units of time then the delta would become
`0000 1101`.
Thus we have removed the largest order from the bit sequence and have become
closer to timeout.
Next we would need to wait `0000 1000` to remove the next largest bit.
This would continue until the current time is the timeout time and thus
we expire the timeout.

This illustrates the idea that we place a timeout into a bin corresponding to
the topmost bit.
Then once the amount of time the bin represents has transpired we
re-evaluate the timeout and place it into a lower bin.
This process repeats until the timeout experiences expiry.


### Start Algorithm
<a name="algorithm-start" />

Each timeout has a static timestamp `t` indicating when in the future it expires.
From the static timestamp and now timestamp we can compute the difference.
Note that if `t` is expired we have achieved the goal and further calculations
are invalid.
If the timeout is outside the range tracked by the bins [0, 30], then
the timeout is placed in the overflow bin [31].
Placing a timeout in the overflow bin is inefficient if the timeout is far
outside the tracked time range; user be warned that this is a bad thing from
a big-Oh perspective.
Now that we've discussed edge cases, the normal case is the timeout is in the
normal range and is disscussed in `ENQUEUE`.

    START(t):
    if t in list: return
    if t is expired:
        add t to expiry list
    otherwise:
        ENQUEUE(t)
    return


### Enqueue Algorithm
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


### Stop Algorithm
<a name="algorithm-stop" />

Stopping a timeout is trivial and only requires removing it from a list
and clearing the bitset indicating which bins are set if that list is empty.

    STOP(t):
    if t in list:
        unlink t from list
        check if list is empty
        clear bitset of bins if empty


### Wait Algorithm
<a name="algorithm-wait" />

    WAIT(now, t):
    last = previous now
    diff = now - last
    tdiff = t - last
    if diff < tdiff
        return tdiff - diff
    otherwise:
        return 0


### Timeout Algorithm
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


### Prove Order
<a name="prove-order" />

Prove that `order(t^n) >= order(delta)`.
Temporarily this fact is intuitively derived.

    Let delta = t - n.

    Since delta + n, which is t, causes the bits to potentially cascade,
    then excluding the same terms between t and n results in an order greater
    than or equal to delta.


## Reading Materials
<a name="reading-materials" />

<a name="wheel-paper" />

George Varghese and Anthony Lack (1997).
Hashed and Hierarchical Timing Wheels: Efficient Data Structures for
Implementing a Timer Facility.
(IEEE/ACM Transactions on Networking, Vol. 5, No.6, December 1997).


## TODO
<a name="todo" />

* [ ] Add a function that updates the timeout stamp, hitime_touch
* [ ] Add a function that will set the timeout expiry time to be within a given
      range of time in the future so as to minimize the times the timeout
      is placed in new bins. hitime_start_range


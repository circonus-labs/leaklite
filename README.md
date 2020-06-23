# Leaklite 
## Bring a light into the memory allocation darkness with lightweight impact to performance

Leaklite is an experimental work-in-progress of a C and C++ real-time memory allocation tracker.  It takes a different approach to other known memory tools, and strives for near zero performance overhead without resorting to sampling (so that you could potentially use it in production).  I had an idea and took it down the road some distance, in the interest of making memory allocation behavior (and leak detection) much easier for my own personal work.  It does not care what allocator you are using, although one experiment leverages non-standard jemalloc APIs to try and get additional performance. It can provide built-in continuous visibility into memory usage by source line where allocations were done.

Leaklite was used in a limited way to catch a few memory leaks during a large project.  I investigated rolling it into libmtev or into jemalloc, but it ends up better for it to stand as a separate project.  Although it can work in conjunction with them, it must be included and built into the binary in order to gain the performance benefits of leveraging static counters and not requiring trampolining.

The source code is contained within a few smallish source files currently that gain a performance benefit by being optionally built into your codebase.  For C++ it requires nothing more than the inclusion of a header file in many cases.  For C it currently requires a small code change at the point of each allocation to insert a macro.  The eventual plan is to create a script or tool which will allow this to be added or removed by a single commaandline. In addition, it is desirable to be able to enable or disable it easily at runtime.

Vasu Raman, 2020

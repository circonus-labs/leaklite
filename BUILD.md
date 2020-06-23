# Building with leaklite

To use leaklite with your project, simply add the source files to your project as your first step.  If you are using the Mt. Everest library (libmtev), you can use the rest_leaklite files as they are.  If not, you can modify them to expose the current memory allocation dump by using any other suitable REST API or other mechanism.

The initial version requires a hash to store pointers in order to ignore frees/deletes which were not instrumented with leaklite.  For thread safety the excellent Concurrency Kit library (http://concurrencykit.org) and a pair of helper files called "pointer_hash" have been used.  Further experiments are being done to try to remove this dependency in the future.

Each C++ and C file that you wish to monitor will need the appropriate ".h" or ".hpp" file included.  Preferably, this #include would be placed at the end of all of the 3rd-party includes but before any includes that you may have code within that perform alloc/new or free/delete.

C++:
```
#include "leaklite.hpp"
```

C:
```
#include "leaklite.h"
```

For C++ it can be as simple as that, unless you are on a fairly old or specialized compiler that does not support C++ lambdas.  In a C source file (or an old C++ compiler) you also need to search for each alloc (malloc, calloc, realloc, new) and add "\_\_LEAKLITE\_\_" at the beginning of the source line.  This will eventually be automated, but for now it allows leaklite to create a high-performance static counter at the location of the allocation.

**For C or non-lambda C++ only:**
```
__LEAKLITE__ char *mystr = (char *)malloc(MYSTR_SIZE);
```

For any locations which cause build issues, you can choose to fix the issue or you can "#pragma push-macro" and "#undef" the macro that leaklite uses to replace allocations (and then redefine them afterwards with "#pragma pop-macro").  For example:

```
#pragma push_macro("free")
#undef free
  my_namespace::my_class::my_subclass::free(some_memory_ptr);
#pragma pop_macro("free")
```

By doing this, you can exclude things like special allocations/frees or places where you allocate memory permanently from leaklite.

Leaklite is in its infancy, and contributions are welcomed.  It is my hope that this process will become a one-step instrument/deinstrument with very little need for manual editing.

Happy leak hunting and allocation profiling!!!

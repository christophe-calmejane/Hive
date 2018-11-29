# Contributing code

## Coding style and guidelines
- Use the provided clang-format file to correctly format the code
  - A patched version of clang-format should be used (until [this patch has been integrated into clang-format](https://reviews.llvm.org/D44609))
  - clang-format version 7.0.0 (with the patch applied: [clang-7.0.0-BraceWrappingBeforeLambdaBody.patch](3rdparty/avdecc/clang-7.0.0-BraceWrappingBeforeLambdaBody.patch))
  - Precompiled clang-format for windows and macOS [can be found here](http://www.kikisoft.com/Hive/clang-format)
- Always put _const_ to the right of the type
- Always initialize non static data members (NSDM) except when impossible (when the data member is a reference), using brace initialization
- Add the _virtual_ keyword when _overriding_ a _virtual method_
- Always declare function parameters as const (so they cannot be changed inside the function). Except for references, movable objects and pointers that are mutable
- Prohibit _C-style cast_, using _static_cast_ instead
- Prohibit _C-style types_ (int, long, char), use _cstdint types_ instead (std::int32_t, std::int8_t)
- Don't use function parameter to return a value (except for in/out _objects_, but never for _simple types_), use the return value of the function (use pair/tuple/struct/optional if required). Except if absolutely required.
- When enumerating a map/unordered_map using a _for loop_, name the iterator loop variable _somethingKV_ (_key/value_)
- [Always use _auto_ for variables declaration](https://youtu.be/xnqTKD8uD64?t=1808) as it's impossible to have an uninitialized variable (possible since c++17 thanks to [[dcl.init] update in P0135R1: Guaranteed copy elision](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0135r1.html))
- Always specify the type to the right of the = sign when declaring a variable (and always use the = sign [for the sake of consistency](https://youtu.be/xnqTKD8uD64?t=2381)). Since c++17 and [dcl.init], there is no downside to this rule.
- [A non-owning pointer passed to a function should be declared as _&_ if required and _*_ if optional](https://youtu.be/xnqTKD8uD64?t=956)
- [Never pass a shared_pointer directly if the function does not participate in ownership (taking a count in the reference)](https://youtu.be/xnqTKD8uD64?t=1034)
- [Never pass to a function a dereferenced non local shared_pointer (sptr.get() or *sptr, where sptr is global or is class member that can be changed by another thread or sub-function). Take a reference count before dereferencing.](https://youtu.be/xnqTKD8uD64?t=1569)

## Submit code
- Create a new branch starting from '*dev*' branch
  - Name it '*issue/#*' if the code is linked to a github issue (replace '*#*' with the github ticket number)
  - Name it '*task/taskName*' if the code is not a github issue (replace '*taskName*' with a clear lowerCamelCase name for the code)
- Add the code in that branch
- Don't forget to update the CHANGELOG file with all useful information for the end-user, as this file is used to automatically generate the **What's New** dialog
- Run the *fix_files.sh* script
- Start a github pull request

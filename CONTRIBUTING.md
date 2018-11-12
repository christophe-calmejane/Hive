# Contributing code

## Coding style
- Use the provided clang-format file to correctly format the code
  - A patched version of clang-format should be used (until [this patch has been integrated into clang-format](https://reviews.llvm.org/D44609))
  - clang-format version 7.0.0 (with the patch applied: [clang-7.0.0-BraceWrappingBeforeLambdaBody.patch](3rdparty/avdecc/clang-7.0.0-BraceWrappingBeforeLambdaBody.patch))
  - Precompiled clang-format for windows and macOS [can be found here](http://www.kikisoft.com/Hive/clang-format)
- Always declare function parameters as const (so they cannot be changed inside the function). Except for references, movable objects and pointers that are mutable
- Always put _const_ to the right of the type
- Almost always use _auto_ for variables
- Prefer _brace initialization_ for variables
- Prohibit _C-style cast_, using _static_cast_ instead
- Don't use function parameter to return a value (except for _objects_ in/out, but never for _simple types_), use the return value of the function (use pair/tuple/struct/optional if required)
- When enumerating a map/unordered_map using a _for loop_, name the iterator loop variable _somethingKV_ (_key/value_)
- Add the _virtual_ keyword when _overriding_ a _virtual method_

## Submit code
- Create a new branch starting from '*dev*' branch
  - Name it '*issue/#*' if the code is linked to a github issue (replace '*#*' with the github ticket number)
  - Name it '*task/taskName*' if the code is not a github issue (replace '*taskName*' with a clear lowerCamelCase name for the code)
- Add the code in that branch
- Don't forget to update the CHANGELOG file with all useful information for the end-user, as this file is used to automatically generate the **What's New** dialog
- Run the *fix_files.sh* script
- Start a github pull request

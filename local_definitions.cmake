# Override the default warning flags.
cu_set_warning_flags(TARGETS QtMate_static COMPILER CLANG PRIVATE -Wno-unused-parameter -Wno-float-conversion) # -Wno-float-conversion to be removed!!
cu_set_warning_flags(TARGETS Hive_static Hive COMPILER CLANG PRIVATE -Wno-unused-parameter)
# cu_set_warning_flags(TARGETS la_avdecc_cxx la_avdecc_static la_avdecc_controller_cxx la_avdecc_controller_static COMPILER CLANG PRIVATE -Wno-unused-parameter PUBLIC -Wno-gnu-zero-variadic-macro-arguments)
# cu_set_warning_flags(TARGETS la_avdecc_cxx la_avdecc_static la_avdecc_controller_cxx la_avdecc_controller_static Tests COMPILER GCC PRIVATE -Wno-ignored-qualifiers)

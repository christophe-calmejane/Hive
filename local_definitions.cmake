# Override the default warning flags.
cu_set_warning_flags(TARGETS QtMate_static COMPILER CLANG PRIVATE -Wno-unused-parameter -Wno-float-conversion) # -Wno-float-conversion to be removed!!
cu_set_warning_flags(TARGETS Hive_static Hive COMPILER CLANG PRIVATE -Wno-unused-parameter)
cu_set_warning_flags(TARGETS QtMate_static COMPILER GCC PRIVATE -Wno-unused-parameter -Wno-ignored-qualifiers -Wno-float-conversion) # -Wno-float-conversion to be removed!!
cu_set_warning_flags(TARGETS Hive_widget_models_static Hive_static COMPILER GCC PRIVATE -Wno-float-conversion) # -Wno-float-conversion to be removed!!

function(CompilerWarningsAsError targetname)
    target_compile_options(${targetname} PRIVATE
            $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:
            -Wall -Wextra -Walloca -Wcast-align -Wcast-qual -Wconditionally-supported -Wconversion -Wdangling-else -Wdouble-promotion -Wduplicated-cond -Wfloat-conversion -Wfloat-equal -Wno-psabi -Wlogical-op -Wno-aggressive-loop-optimizations -Wnull-dereference -Wredundant-decls -Wshadow -Wshadow=compatible-local -Wstack-protector -Wsuggest-override -Wswitch-bool -Wundef -Wunsafe-loop-optimizations -Wuseless-cast -Wvla -Werror -pedantic -Wpedantic -Wno-error=unsafe-loop-optimizations -Wno-zero-as-null-pointer-constant> # for old gcc and boost
            $<$<CXX_COMPILER_ID:MSVC>:
            /WX>)
endfunction()

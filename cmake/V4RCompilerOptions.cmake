if(X86)
  # x86 compiler is known to produce unstable SSE code with -O3 hence we are trying to use -O2 instead
  if(CMAKE_COMPILER_IS_GNUCXX)
    foreach(flags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG)
      string(REPLACE "-O3" "-O2" ${flags} "${${flags}}")
    endforeach()
  endif()

  if(CMAKE_COMPILER_IS_GNUCC)
    foreach(flags CMAKE_C_FLAGS CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG)
      string(REPLACE "-O3" "-O2" ${flags} "${${flags}}")
    endforeach()
  endif()
endif()

set(V4R_EXTRA_FLAGS "")
set(V4R_EXTRA_C_FLAGS "")
set(V4R_EXTRA_CXX_FLAGS "")
set(V4R_EXTRA_FLAGS_RELEASE "")
set(V4R_EXTRA_FLAGS_DEBUG "")
set(V4R_EXTRA_EXE_LINKER_FLAGS "")
set(V4R_EXTRA_EXE_LINKER_FLAGS_RELEASE "")
set(V4R_EXTRA_EXE_LINKER_FLAGS_DEBUG "")

macro(add_extra_compiler_option option)
  if(CMAKE_BUILD_TYPE)
    set(CMAKE_TRY_COMPILE_CONFIGURATION ${CMAKE_BUILD_TYPE})
  endif()
  v4r_check_flag_support(CXX "${option}" _varname "${V4R_EXTRA_CXX_FLAGS} ${ARGN}")
  if(${_varname})
    set(V4R_EXTRA_CXX_FLAGS "${V4R_EXTRA_CXX_FLAGS} ${option}")
  endif()

  v4r_check_flag_support(C "${option}" _varname "${V4R_EXTRA_C_FLAGS} ${ARGN}")
  if(${_varname})
    set(V4R_EXTRA_C_FLAGS "${V4R_EXTRA_C_FLAGS} ${option}")
  endif()
endmacro()

# Gets environment variable and puts its value to the corresponding preprocessor definition
# Useful for WINRT that has no access to environment variables
macro(add_env_definitions option)
  set(value $ENV{${option}})
  if("${value}" STREQUAL "")
    message(WARNING "${option} environment variable is empty. Please set it to appropriate location to get correct results")
  else()
    string(REPLACE "\\" "\\\\" value ${value})
  endif()
  add_definitions("-D${option}=\"${value}\"")
endmacro()

if(CMAKE_COMPILER_IS_GNUCXX)
  # High level of warnings.
  # TODO: some are commented out to make V4R compile with least effort [SA 2015-07-29]
  add_extra_compiler_option(-W)
  add_extra_compiler_option(-Wall)
  add_extra_compiler_option(-Werror=return-type)
  #add_extra_compiler_option(-Werror=non-virtual-dtor)
  add_extra_compiler_option(-Werror=address)
  add_extra_compiler_option(-Werror=sequence-point)
  add_extra_compiler_option(-Wformat)
  add_extra_compiler_option(-Werror=format-security -Wformat)
  add_extra_compiler_option(-Wmissing-declarations)
  add_extra_compiler_option(-Wmissing-prototypes)
  add_extra_compiler_option(-Wstrict-prototypes)
  #add_extra_compiler_option(-Wundef)
  add_extra_compiler_option(-Winit-self)
  add_extra_compiler_option(-Wpointer-arith)
  add_extra_compiler_option(-Wshadow)
  add_extra_compiler_option(-Wsign-promo)

  if(ENABLE_NOISY_WARNINGS)
    add_extra_compiler_option(-Wcast-align)
    add_extra_compiler_option(-Wstrict-aliasing=2)
  else()
    add_extra_compiler_option(-Wno-narrowing)
    add_extra_compiler_option(-Wno-delete-non-virtual-dtor)
    add_extra_compiler_option(-Wno-unnamed-type-template-args)
  endif()
  add_extra_compiler_option(-fdiagnostics-show-option)

  # The -Wno-long-long is required in 64bit systems when including sytem headers.
  if(X86_64)
    add_extra_compiler_option(-Wno-long-long)
  endif()

  # We need pthread's
  add_extra_compiler_option(-pthread)

  if(CMAKE_COMPILER_IS_CLANGCXX)
    add_extra_compiler_option(-Qunused-arguments)
  endif()

  if(V4R_WARNINGS_ARE_ERRORS)
    add_extra_compiler_option(-Werror)
  endif()

  if(X86 AND NOT X86_64)
    add_extra_compiler_option(-march=i686)
  endif()

  # Other optimizations
  if(ENABLE_OMIT_FRAME_POINTER)
    add_extra_compiler_option(-fomit-frame-pointer)
  else()
    add_extra_compiler_option(-fno-omit-frame-pointer)
  endif()
  if(ENABLE_FAST_MATH)
    add_extra_compiler_option(-ffast-math)
  endif()
  if(ENABLE_POWERPC)
    add_extra_compiler_option("-mcpu=G3 -mtune=G5")
  endif()
  if(ENABLE_SSE)
    add_extra_compiler_option(-msse)
  endif()
  if(ENABLE_SSE2)
    add_extra_compiler_option(-msse2)
  elseif(X86 OR X86_64)
    add_extra_compiler_option(-mno-sse2)
  endif()

  if(ENABLE_AVX)
    add_extra_compiler_option(-mavx)
  elseif(X86 OR X86_64)
    add_extra_compiler_option(-mno-avx)
  endif()
  if(ENABLE_AVX2)
    add_extra_compiler_option(-mavx2)
    if(ENABLE_FMA3)
      add_extra_compiler_option(-mfma)
    endif()
  endif()

  # GCC depresses SSEx instructions when -mavx is used. Instead, it generates new AVX instructions or AVX equivalence for all SSEx instructions when needed.
  if(NOT V4R_EXTRA_CXX_FLAGS MATCHES "-mavx")
    if(ENABLE_SSE3)
      add_extra_compiler_option(-msse3)
    elseif(X86 OR X86_64)
      add_extra_compiler_option(-mno-sse3)
    endif()

    if(ENABLE_SSSE3)
      add_extra_compiler_option(-mssse3)
    elseif(X86 OR X86_64)
      add_extra_compiler_option(-mno-ssse3)
    endif()

    if(ENABLE_SSE41)
      add_extra_compiler_option(-msse4.1)
    elseif(X86 OR X86_64)
      add_extra_compiler_option(-mno-sse4.1)
    endif()

    if(ENABLE_SSE42)
      add_extra_compiler_option(-msse4.2)
    elseif(X86 OR X86_64)
      add_extra_compiler_option(-mno-sse4.2)
    endif()

    if(ENABLE_POPCNT)
      add_extra_compiler_option(-mpopcnt)
    endif()
  endif()

  if(X86 OR X86_64)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
      if(V4R_EXTRA_CXX_FLAGS MATCHES "-m(sse2|avx)")
        add_extra_compiler_option(-mfpmath=sse)# !! important - be on the same wave with x64 compilers
      else()
        add_extra_compiler_option(-mfpmath=387)
      endif()
    endif()
  endif()

  # Profiling?
  if(ENABLE_PROFILING)
    add_extra_compiler_option("-pg -g")
    # turn off incompatible options
    foreach(flags CMAKE_CXX_FLAGS CMAKE_C_FLAGS CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG
                  V4R_EXTRA_FLAGS_RELEASE V4R_EXTRA_FLAGS_DEBUG V4R_EXTRA_C_FLAGS V4R_EXTRA_CXX_FLAGS)
      string(REPLACE "-fomit-frame-pointer" "" ${flags} "${${flags}}")
      string(REPLACE "-ffunction-sections" "" ${flags} "${${flags}}")
    endforeach()
  else()
    # Remove unreferenced functions: function level linking
    add_extra_compiler_option(-ffunction-sections)
  endif()

  if(ENABLE_COVERAGE)
    set(V4R_EXTRA_C_FLAGS "${V4R_EXTRA_C_FLAGS} --coverage")
    set(V4R_EXTRA_CXX_FLAGS "${V4R_EXTRA_CXX_FLAGS} --coverage")
  endif()

  set(V4R_EXTRA_FLAGS_RELEASE "${V4R_EXTRA_FLAGS_RELEASE} -DNDEBUG")
  set(V4R_EXTRA_FLAGS_DEBUG "${V4R_EXTRA_FLAGS_DEBUG} -O0 -DDEBUG -D_DEBUG")
endif()

# Extra link libs if the user selects building static libs:
if(NOT BUILD_SHARED_LIBS AND CMAKE_COMPILER_IS_GNUCXX)
  set(V4R_LINKER_LIBS ${V4R_LINKER_LIBS} stdc++)
  set(V4R_EXTRA_FLAGS "-fPIC ${V4R_EXTRA_FLAGS}")
endif()

# Add user supplied extra options (optimization, etc...)
# ==========================================================
set(V4R_EXTRA_FLAGS         "${V4R_EXTRA_FLAGS}"         CACHE INTERNAL "Extra compiler options")
set(V4R_EXTRA_C_FLAGS       "${V4R_EXTRA_C_FLAGS}"       CACHE INTERNAL "Extra compiler options for C sources")
set(V4R_EXTRA_CXX_FLAGS     "${V4R_EXTRA_CXX_FLAGS}"     CACHE INTERNAL "Extra compiler options for C++ sources")
set(V4R_EXTRA_FLAGS_RELEASE "${V4R_EXTRA_FLAGS_RELEASE}" CACHE INTERNAL "Extra compiler options for Release build")
set(V4R_EXTRA_FLAGS_DEBUG   "${V4R_EXTRA_FLAGS_DEBUG}"   CACHE INTERNAL "Extra compiler options for Debug build")
set(V4R_EXTRA_EXE_LINKER_FLAGS         "${V4R_EXTRA_EXE_LINKER_FLAGS}"         CACHE INTERNAL "Extra linker flags")
set(V4R_EXTRA_EXE_LINKER_FLAGS_RELEASE "${V4R_EXTRA_EXE_LINKER_FLAGS_RELEASE}" CACHE INTERNAL "Extra linker flags for Release build")
set(V4R_EXTRA_EXE_LINKER_FLAGS_DEBUG   "${V4R_EXTRA_EXE_LINKER_FLAGS_DEBUG}"   CACHE INTERNAL "Extra linker flags for Debug build")

# set default visibility to hidden
if(CMAKE_COMPILER_IS_GNUCXX AND CMAKE_V4R_GCC_VERSION_NUM GREATER 399)
  add_extra_compiler_option(-fvisibility=hidden)
  add_extra_compiler_option(-fvisibility-inlines-hidden)
endif()

#combine all "extra" options
set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} ${V4R_EXTRA_FLAGS} ${V4R_EXTRA_C_FLAGS}")
set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} ${V4R_EXTRA_FLAGS} ${V4R_EXTRA_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${V4R_EXTRA_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE} ${V4R_EXTRA_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} ${V4R_EXTRA_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG} ${V4R_EXTRA_FLAGS_DEBUG}")
set(CMAKE_EXE_LINKER_FLAGS         "${CMAKE_EXE_LINKER_FLAGS} ${V4R_EXTRA_EXE_LINKER_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} ${V4R_EXTRA_EXE_LINKER_FLAGS_RELEASE}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${V4R_EXTRA_EXE_LINKER_FLAGS_DEBUG}")

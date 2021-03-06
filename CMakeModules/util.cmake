function(lgd_print_options)
  message("-- The following options have been configured:")
  message(${OPTIONS_LIST})
  message("")
endfunction(lgd_print_options)

function(lgd_dump_config outfile)
  set(CONTENT "#ifndef LIBGEODECOMP_CONFIG_H\n\n")
  set(CONTENT "${CONTENT}${CONFIG_HEADER}\n")
  set(CONTENT "${CONTENT}#endif\n")
  set(PATHNAME "${CMAKE_BINARY_DIR}/libgeodecomp/${outfile}")
  file(WRITE "${PATHNAME}.new" "${CONTENT}")

  execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/libgeodecomp")
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files  "${PATHNAME}" "${PATHNAME}.new" RESULT_VARIABLE res)
  if(res GREATER 0)
    file(WRITE "${PATHNAME}" "${CONTENT}")
  endif()

  file(REMOVE "${PATHNAME}.new")
endfunction(lgd_dump_config)

# generic function to add user-configurable options. add_to_header may be used to propagate the option to a header file.
function(lgd_add_config_option name help_message default add_to_header)
  if(NOT DEFINED ${name})
    set(${name} "${default}")
    set(${name} "${default}" CACHE STRING "${help_message}" FORCE)
  endif()

  set(OPTIONS_LIST "${OPTIONS_LIST}\n\n * ${name}=\"${${name}}\",\n   default=\"${default}\"\n   ${help_message}" PARENT_SCOPE)

  if(add_to_header)
    if(${name})
      set(CONFIG_HEADER "${CONFIG_HEADER}#define LIBGEODECOMP_${name} ${${name}}\n" PARENT_SCOPE)
    endif()
  endif()
endfunction(lgd_add_config_option)

# list headers/source files in "auto.cmake"
function(lgd_generate_sourcelists relative_dir)
  get_filename_component(dir ${relative_dir} ABSOLUTE)

  file(GLOB RAW_SOURCES "${dir}/*.cu" "${dir}/*.cpp" "${dir}/*.f")
  file(GLOB RAW_HEADERS "${dir}/*.h")

  if(RAW_SOURCES OR RAW_HEADERS)
    set(STRIPPED_SOURCES)
    set(STRIPPED_HEADERS)

    foreach(i ${RAW_SOURCES})
      get_filename_component(name ${i} NAME)
      list(APPEND STRIPPED_SOURCES ${name})
    endforeach(i)

    foreach(i ${RAW_HEADERS})
      get_filename_component(name ${i} NAME)
      list(APPEND STRIPPED_HEADERS ${name})
    endforeach(i)

    if(STRIPPED_SOURCES)
      list(SORT STRIPPED_SOURCES)
    endif(STRIPPED_SOURCES)

    if(STRIPPED_HEADERS)
      list(SORT STRIPPED_HEADERS)
    endif(STRIPPED_HEADERS)

    set(MY_AUTO "set(SOURCES \${SOURCES}\n")
    foreach(i ${STRIPPED_SOURCES})
      set(MY_AUTO "${MY_AUTO}  \${RELATIVE_PATH}${i}\n")
    endforeach(i)

    set(MY_AUTO "${MY_AUTO})\nset(HEADERS \${HEADERS}\n")
    foreach(i ${STRIPPED_HEADERS})
      set(MY_AUTO "${MY_AUTO}  \${RELATIVE_PATH}${i}\n")
    endforeach(i)
    set(MY_AUTO "${MY_AUTO})\n")

    # only actually write the file if it differs
    if(NOT EXISTS "${dir}/auto.cmake")
      set(regen_auto 1)
    endif(NOT EXISTS "${dir}/auto.cmake")

    if(EXISTS "${dir}/auto.cmake")
      file(READ "${dir}/auto.cmake" PREV_AUTO)
      string(COMPARE NOTEQUAL ${MY_AUTO} ${PREV_AUTO} regen_auto)
    endif(EXISTS "${dir}/auto.cmake")

    if (regen_auto)
      message("updating ${dir}//auto.cmake")
      file(WRITE "${dir}/auto.cmake" ${MY_AUTO})
    endif(regen_auto)
  endif(RAW_SOURCES OR RAW_HEADERS)
endfunction(lgd_generate_sourcelists)

# creates a string constant from a source file, handy for e.g.
# just-in-time compilation of OpenCL kernels
function(lgd_escape_kernel outfile infile)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/${outfile}"
    COMMAND cat "${CMAKE_CURRENT_SOURCE_DIR}/${infile}" | sed 's/"/\\\\"/g' | sed s/.*/\\"\\&\\\\\\\\n\\"/ >"${CMAKE_CURRENT_SOURCE_DIR}/${outfile}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${infile}"
    )
endfunction(lgd_escape_kernel)

function(lgd_detect_distro)
  if(EXISTS "/etc/lsb-release")
    file(READ "/etc/lsb-release" lsb_release)

    if(lsb_release MATCHES "Gentoo")
      set(distro "Gentoo")
    endif()

    if(lsb_release MATCHES "core")
      set(distro "Cray")
    endif()
  endif()

  if(NOT DEFINED distro)
    if(EXISTS "/etc/fedora-release")
      set(distro "Fedora")
    endif()
  endif()

  if(NOT DEFINED distro)
    if(EXISTS "/etc/debian_version")
      set(distro "Debian")
    endif()
  endif()

  if(NOT DEFINED distro)
    set(distro "Unknown")
  endif()

  set(distro ${distro} PARENT_SCOPE)
endfunction(lgd_detect_distro)

# - G4agent category build
#
# GEANT4_USE_AGENT_LLM (OFF by default) adds the optional G4agent_llm
# module: an OpenAI-compatible HTTP backend linking libcurl. The core
# library contains no network code and never links this module; it is
# resolved at run time (see G4AgentLLMFactory.cc). 
option(GEANT4_USE_AGENT_LLM "Build the Geant4 agent HTTP LLM backend (libcurl)" OFF)
# Keep the agent options visible in ccmake/cmake-gui: they are the two knobs a
# user is expected to find. CLEAR also un-hides them in an existing build tree
# whose cache already recorded the old advanced state.
mark_as_advanced(CLEAR GEANT4_USE_AGENT_LLM)

# - The g4 command-line harness. It is a thin front end over the native
#   G4agent services and installs to <prefix>/bin/g4, so it is available
#   after sourcing the Geant4 environment script. Option-scoped (default ON)
#   so that introducing it does not disrupt ordinary toolkit builds.
option(GEANT4_USE_AGENT_CLI "Build and install the g4 agent CLI executable" ON)
mark_as_advanced(CLEAR GEANT4_USE_AGENT_CLI)

include(sources.cmake)

if(GEANT4_USE_AGENT_LLM)
  find_package(CURL QUIET)
  if(NOT CURL_FOUND)
    message(WARNING
      "GEANT4_USE_AGENT_LLM=ON but libcurl was not found; "
      "the G4agent_llm HTTP backend will not be built.")
  endif()
endif()

if(CURL_FOUND)
  include(llm/sources.cmake)
endif()

geant4_add_category(G4agent MODULES G4agent)

if(CURL_FOUND)
  geant4_add_category(G4agent_llm MODULES G4agent_llm)
endif()

if(GEANT4_USE_AGENT_CLI)
  # The g4 executable links the agent category and the run/UI kernel it
  # observes. G4agent PUBLIC-links the global and intercoms categories, so
  # the transitive link interface supplies G4String, G4UImanager, etc.
  add_executable(g4 tools/g4.cc)
  target_link_libraries(g4 PRIVATE G4agent G4run)
  # When the optional HTTP backend is built, link it into the CLI so that
  # the backend factory can resolve G4AgentLLMCreateHTTP via
  # dlsym(RTLD_DEFAULT). --no-as-needed keeps the library in the process
  # image even though the CLI references none of its symbols directly.
  if(CURL_FOUND)
    target_link_libraries(g4 PRIVATE "-Wl,--no-as-needed" G4agent_llm)
  endif()
  set_target_properties(g4 PROPERTIES
    OUTPUT_NAME g4)
  install(TARGETS g4
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    COMPONENT Runtime)
endif()

# - G4agent_llm module: optional OpenAI-compatible HTTP backend (libcurl)

geant4_add_module(G4agent_llm
  SOURCES
    G4AgentLLMOpenAI.cc)

geant4_module_link_libraries(G4agent_llm
  PUBLIC
    G4agent
  PRIVATE
    ${CURL_LIBRARIES})

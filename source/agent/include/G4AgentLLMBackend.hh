//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications, and indicate your  *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
// G4AgentLLMBackend
//
// Class description
//
// Provider-neutral LLM client abstraction for the agent. A backend sends a chat
// completion request (optionally with tool specifications) and returns a
// neutral response object. It contains no Geant4 knowledge and no vendor
// assumptions spread through the agent.
//
// Required capability set: send request, receive response,
// tool-call reporting, configurable model/provider, timeout/error handling
// surfaced as a deterministic error state (never an exception).
//
// The concrete OpenAI-compatible HTTP backend is a separate optional
// module (G4agent_llm, GEANT4_USE_AGENT_LLM). The core library ships only
// this interface, the environment configuration, and a deterministic fake
// used by tests and by users who want the loop exercised offline.

#ifndef G4AGENTLLMBACKEND_HH
#define G4AGENTLLMBACKEND_HH

#include "G4String.hh"

#include <vector>

// Chat message roles
enum class G4LLMRole
{
  System,
  User,
  Assistant,
  // Result of a tool call, associated via fToolCallId
  Tool
};

// A tool call requested by the model, executed by the agent's semantic
// tools. Tools take a single optional text argument (matching the UI
// command surface).
struct G4LLMToolCall
{
    G4String fId;
    G4String fName;
    G4String fArgument;
};

struct G4LLMMessage
{
    G4LLMRole fRole = G4LLMRole::User;
    G4String fContent;
    G4String fToolCallId;
    // Assistant messages may carry the tool calls they requested.
    std::vector<G4LLMToolCall> fToolCalls;
};

struct G4LLMToolSpec
{
    G4String fName;
    G4String fDescription;
};

// Neutral response. On failure fOk is false and fError carries a
// deterministic, human-readable message (timeout, connection failure,
// malformed response, ...). No exception escapes a backend call.
struct G4LLMReply
{
    G4bool fOk = false;
    G4String fError;
    G4String fText;
    std::vector<G4LLMToolCall> fToolCalls;
    // "stop", "tool_calls", or provider-specific values
    G4String fFinishReason;
};

class G4AgentLLMBackend
{
  public:

    virtual ~G4AgentLLMBackend() = default;

    // Short provider-neutral backend identifier, e.g. "fake", "openai".
    virtual G4String Name() const = 0;

    // Single completion request. The backend must be thread-safe only with
    // respect to its own state; the agent calls it from the master thread.
    virtual G4LLMReply Complete(const std::vector<G4LLMMessage>& messages,
                                const std::vector<G4LLMToolSpec>& tools) = 0;
};

#endif

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

#include "G4AgentLLMFake.hh"

#include <cstdlib>

G4AgentLLMFake::G4AgentLLMFake()
{
  const char* script = std::getenv("G4_AGENT_FAKE_SCRIPT");
  if (script != nullptr)
  {
    // Optional repeat factor: replicate the parsed script N times, useful
    // for driving the reasoning loop to its step limit deterministically.
    const char* rep = std::getenv("G4_AGENT_FAKE_REPEAT");

    // Steps separated by '|' (not ';' which collides with list separators
    // in shell/ctest environments).
    G4String text(script);
    std::size_t start = 0;
    while (start < text.size())
    {
      std::size_t sep = text.find('|', start);
      if (sep == G4String::npos) sep = text.size();
      G4String line = text.substr(start, sep - start);
      // trim leading spaces
      std::size_t b = line.find_first_not_of(" \t");
      if (b != G4String::npos) fScript.push_back(line.substr(b));
      start = sep + 1;
    }
    if (rep != nullptr)
    {
      const int factor = std::atoi(rep);
      if (factor > 1)
      {
        std::vector<G4String> base = fScript;
        for (int r = 1; r < factor; ++r)
          for (const auto& line : base)
            fScript.push_back(line);
      }
    }
  }
}

void G4AgentLLMFake::SetScript(const std::vector<G4String>& lines)
{
  fScript = lines;
  fStep = 0;
}

G4LLMReply G4AgentLLMFake::Complete(const std::vector<G4LLMMessage>& messages,
                                    const std::vector<G4LLMToolSpec>& tools)
{
  (void)messages;
  (void)tools;

  G4LLMReply reply;
  reply.fOk = true;

  if (fStep >= G4int(fScript.size()))
  {
    reply.fText = "no further steps";
    return reply;
  }

  const G4String& line = fScript[fStep];
  if (line == "fail")
  {
    // Deterministic scripted failure for loop/error tests
    reply.fOk = false;
    reply.fError = "scripted failure";
    ++fStep;
    return reply;
  }

  if (line.compare(0, 5, "tool ") == 0)
  {
    // tool <name>[ <argument>]
    G4String payload = line.substr(5);
    std::size_t sp = payload.find(' ');
    G4LLMToolCall call;
    if (sp == G4String::npos)
    {
      call.fName = payload;
    }
    else
    {
      call.fName = payload.substr(0, sp);
      call.fArgument = payload.substr(sp + 1);
      // trim leading spaces of the argument
      std::size_t b = call.fArgument.find_first_not_of(' ');
      call.fArgument = (b == G4String::npos) ? G4String("") : call.fArgument.substr(b);
    }
    call.fId = "call_" + std::to_string(fStep);
    reply.fToolCalls.push_back(call);
    reply.fText = "requesting tool " + call.fName;
    reply.fFinishReason = "tool_calls";
    ++fStep;
    return reply;
  }

  if (line.compare(0, 6, "final ") == 0)
  {
    reply.fText = line.substr(6);
    reply.fFinishReason = "stop";
    ++fStep;
    return reply;
  }

  if (line == "final")
  {
    reply.fText = "";
    ++fStep;
    return reply;
  }

  // Unknown script line: deterministic error, still advances
  reply.fOk = false;
  reply.fError = "unrecognized script line: '" + line + "'";
  ++fStep;
  return reply;
}

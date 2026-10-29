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

#include "G4AgentValidate.hh"

#include "G4AgentReport.hh"
#include "G4RunManager.hh"
#include "G4StateManager.hh"

#include <cctype>
#include <string>

namespace
{

enum class Verdict
{
  Pass,
  Warning,
  Fail,
  NotAvailable,
  Insufficient
};

G4String Name(Verdict v)
{
  switch (v)
  {
    case Verdict::Pass:
      return "PASS";
    case Verdict::Warning:
      return "WARNING";
    case Verdict::Fail:
      return "FAIL";
    case Verdict::NotAvailable:
      return "NOT AVAILABLE";
    default:
      return "INSUFFICIENT EVIDENCE";
  }
}

Verdict Worst(Verdict a, Verdict b)
{
  const int rankA = (a == Verdict::Fail)           ? 4
                    : (a == Verdict::Warning)      ? 3
                    : (a == Verdict::NotAvailable) ? 2
                    : (a == Verdict::Insufficient) ? 1
                                                   : 0;
  const int rankB = (b == Verdict::Fail)           ? 4
                    : (b == Verdict::Warning)      ? 3
                    : (b == Verdict::NotAvailable) ? 2
                    : (b == Verdict::Insufficient) ? 1
                                                   : 0;
  return rankB > rankA ? b : a;
}

// The check blocks are composed of "label : VERDICT ..." lines using the
// exact vocabulary; classify each line value and take the
// worst verdict seen in the block.
Verdict Classify(const G4String& block)
{
  Verdict worst = Verdict::Pass;
  bool any = false;
  std::size_t start = 0;
  while (start < block.size())
  {
    std::size_t nl = block.find('\n', start);
    if (nl == G4String::npos) nl = block.size();
    G4String line = block.substr(start, nl - start);
    start = nl + 1;
    auto sep = line.find(" : ");
    if (sep == G4String::npos) continue;
    G4String value(line.substr(sep + 3));
    while (!value.empty() && value[0] == ' ')
      value = value.substr(1);
    if (value.rfind("PASS", 0) == 0)
      worst = Worst(worst, Verdict::Pass);
    else if (value.rfind("FAIL", 0) == 0)
      worst = Worst(worst, Verdict::Fail);
    else if (value.rfind("WARNING", 0) == 0)
      worst = Worst(worst, Verdict::Warning);
    else if (value.rfind("NOT AVAILABLE", 0) == 0)
      worst = Worst(worst, Verdict::NotAvailable);
    else
      continue;
    any = true;
  }
  if (!any) return Verdict::NotAvailable;
  return worst;
}

// First non-numeric token of a label value (e.g. "current run id : none").
G4String FirstToken(const G4String& block, const G4String& label, const G4String& def)
{
  auto pos = block.find(label);
  if (pos == G4String::npos) return def;
  auto sep = block.find(':', pos + label.size());
  if (sep == G4String::npos) return def;
  auto start = sep + 1;
  while (start < block.size() && (block[start] == ' ' || block[start] == '\t'))
    ++start;
  auto end = start;
  while (end < block.size() && block[end] != '\n' && block[end] != ' ')
    ++end;
  return (end == start) ? def : G4String(block.substr(start, end - start));
}

G4String FirstValue(const G4String& block, const G4String& label)
{
  // Same padded-label handling as FirstToken, digits only.
  auto pos = block.find(label);
  if (pos == G4String::npos) return G4String("0");
  auto sep = block.find(':', pos + label.size());
  if (sep == G4String::npos) return G4String("0");
  std::size_t start = sep + 1;
  while (start < block.size() && block[start] == ' ')
    ++start;
  std::size_t end = start;
  while (end < block.size() && std::isdigit((unsigned char)block[end]))
    ++end;
  return (end == start) ? G4String("0") : G4String(block.substr(start, end - start));
}

bool Has(const G4String& tier, const char* keyword)
{
  return tier.find(keyword) != G4String::npos;
}

}  // namespace

G4AgentValidate::G4AgentValidate(const G4AgentCheck& checks, const G4AgentInspect& inspect)
  : fChecks(checks), fInspect(inspect)
{}

G4String G4AgentValidate::Validate(const G4String& tier) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] validate");

  G4String t = tier;
  if (t.empty()) t = "configuration runtime";
  const bool wantConfig = Has(t, "config");
  const bool wantRuntime = Has(t, "run") || Has(t, "runtime") || Has(t, "stat");
  if (!wantConfig && !wantRuntime)
  {
    report.AddLine("error : unknown tier '" + tier + "' (configuration|runtime)");
    return report.AsString();
  }

  const G4String geom = fChecks.GeometryCheck("world");
  const G4String phys = fChecks.PhysicsCheck();
  const G4String run = fChecks.RunCheck();
  const G4String runFields = fInspect.RunFields();
  const G4String runId = FirstToken(runFields, "current run id", G4String("none"));
  const G4String events = FirstValue(runFields, "events in run");
  const bool runPresent = runId != "none";

  const bool nothingInitialized = geom.find("NOT AVAILABLE") != G4String::npos && !runPresent;

  if (wantConfig)
  {
    if (nothingInitialized)
    {
      report.AddLine("geometry : INSUFFICIENT EVIDENCE");
      report.AddLine("physics  : INSUFFICIENT EVIDENCE");
      report.AddLine(
        "note     : no configured application state to validate;"
        " initialize geometry/physics first");
    }
    else
    {
      report.AddLine("geometry : " + Name(Classify(geom)));
      report.AddLine("physics  : " + Name(Classify(phys)));
      report.AddLine("run mgr  : " + Name(Classify(run)));
    }
  }

  Verdict runtimeVerdict = Verdict::NotAvailable;
  if (wantRuntime)
  {
    if (!runPresent)
    {
      runtimeVerdict = nothingInitialized ? Verdict::Insufficient : Verdict::NotAvailable;
      report.AddLine("run      : none");
      report.AddLine("runtime  : "
                     + (nothingInitialized ? G4String("INSUFFICIENT EVIDENCE")
                                           : G4String("NOT AVAILABLE (no completed run)")));
    }
    else
    {
      runtimeVerdict = Verdict::Pass;
      report.AddLine("run      : id " + runId + ", events " + events);
      report.AddLine("runtime  : PASS (a run observed events in this process)");
      report.AddLine("statistics : INSUFFICIENT EVIDENCE");
      report.AddLine(
        "note     : per-run uncertainty/dispersion data are not"
        " exposed by this process; statistical and reference"
        " validation require external analysis (VALIDATION.md)");
    }
  }

  Verdict overall = Verdict::Pass;
  if (nothingInitialized)
    overall = Verdict::Insufficient;
  else
  {
    if (wantConfig)
    {
      overall = Worst(Worst(Classify(geom), Classify(phys)), Classify(run));
    }
    if (wantRuntime) overall = Worst(overall, runtimeVerdict);
  }

  report.AddLine("overall  : " + Name(overall));
  report.AddLine(
    "scope    : mechanical validation of configuration + runtime"
    " evidence; NEVER scientific validation");
  return report.AsString();
}

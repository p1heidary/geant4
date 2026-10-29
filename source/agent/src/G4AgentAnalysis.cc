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
// *                                                                  *
// ********************************************************************
//

#include "G4AgentAnalysis.hh"

#include "G4AgentReport.hh"
#include "G4Filesystem.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

namespace
{
G4fs::path P(const G4String& x)
{
  return G4fs::path{std::string{x}};
}

constexpr std::size_t kMaxFileBytes = 4 * 1024 * 1024;

G4String Trim(const G4String& s)
{
  std::size_t a = 0;
  std::size_t b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a])) != 0)
    ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])) != 0)
    --b;
  return s.substr(a, b - a);
}

G4String ToLower(G4String s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

G4String Num(double v)
{
  std::ostringstream os;
  os << std::setprecision(6) << v;
  return G4String(os.str());
}

// A config file is a sequence of blank-line-separated blocks of `key : value`
// lines; '#' starts a comment. Returns one map per block.
std::vector<std::map<G4String, G4String>> ParseBlocks(const std::string& text)
{
  std::vector<std::map<G4String, G4String>> blocks;
  std::map<G4String, G4String> current;
  std::istringstream in(text);
  std::string raw;
  while (std::getline(in, raw))
  {
    G4String line = Trim(G4String(raw));
    if (line.empty())
    {
      if (!current.empty())
      {
        blocks.push_back(current);
        current.clear();
      }
      continue;
    }
    if (line[0] == '#') continue;
    const auto colon = line.find(':');
    if (colon == G4String::npos) continue;
    G4String key = ToLower(Trim(line.substr(0, colon)));
    G4String value = Trim(line.substr(colon + 1));
    if (!key.empty()) current[key] = value;
  }
  if (!current.empty()) blocks.push_back(current);
  return blocks;
}

G4String Lookup(const std::map<G4String, G4String>& block, const G4String& key)
{
  auto it = block.find(key);
  return it != block.end() ? it->second : G4String("");
}

bool ReadWholeFile(const G4String& absPath, std::string& out)
{
  std::ifstream in(std::string{absPath}, std::ios::binary);
  if (!in) return false;
  out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  if (out.size() > kMaxFileBytes) out.resize(kMaxFileBytes);
  return true;
}
}  // namespace

G4AgentAnalysis::G4AgentAnalysis(const G4AgentProject& project) : fProject(project) {}

G4String G4AgentAnalysis::VerdictName(G4AgentVerdict verdict)
{
  switch (verdict)
  {
    case G4AgentVerdict::Consistent:
      return "CONSISTENT";
    case G4AgentVerdict::Contrary:
      return "CONTRARY";
    case G4AgentVerdict::Inconclusive:
      return "INCONCLUSIVE";
    default:
      return "NOT COMPARABLE";
  }
}

G4bool G4AgentAnalysis::NormalizeUnit(const G4String& unit, G4String& canonicalUnit, double& factor)
{
  const G4String u = ToLower(Trim(unit));
  // Dimensionless
  if (u.empty() || u == "1" || u == "count" || u == "counts" || u == "event" || u == "events"
      || u == "unitless" || u == "dimensionless" || u == "fraction")
  {
    canonicalUnit = "1";
    factor = 1.0;
    return true;
  }
  if (u == "%" || u == "percent" || u == "percentage")
  {
    canonicalUnit = "1";
    factor = 0.01;
    return true;
  }
  // Energy (canonical eV)
  if (u == "ev")
  {
    canonicalUnit = "eV";
    factor = 1.0;
    return true;
  }
  if (u == "kev")
  {
    canonicalUnit = "eV";
    factor = 1e3;
    return true;
  }
  if (u == "mev")
  {
    canonicalUnit = "eV";
    factor = 1e6;
    return true;
  }
  if (u == "gev")
  {
    canonicalUnit = "eV";
    factor = 1e9;
    return true;
  }
  if (u == "tev")
  {
    canonicalUnit = "eV";
    factor = 1e12;
    return true;
  }
  // Length (canonical mm)
  if (u == "nm")
  {
    canonicalUnit = "mm";
    factor = 1e-6;
    return true;
  }
  if (u == "um" || u == "micron")
  {
    canonicalUnit = "mm";
    factor = 1e-3;
    return true;
  }
  if (u == "mm")
  {
    canonicalUnit = "mm";
    factor = 1.0;
    return true;
  }
  if (u == "cm")
  {
    canonicalUnit = "mm";
    factor = 10.0;
    return true;
  }
  if (u == "m")
  {
    canonicalUnit = "mm";
    factor = 1000.0;
    return true;
  }
  // Time (canonical s)
  if (u == "ns")
  {
    canonicalUnit = "s";
    factor = 1e-9;
    return true;
  }
  if (u == "us")
  {
    canonicalUnit = "s";
    factor = 1e-6;
    return true;
  }
  if (u == "ms")
  {
    canonicalUnit = "s";
    factor = 1e-3;
    return true;
  }
  if (u == "s" || u == "sec")
  {
    canonicalUnit = "s";
    factor = 1.0;
    return true;
  }
  // Density (canonical g/cm3)
  if (u == "g/cm3" || u == "g/cm^3" || u == "g cm-3" || u == "gcm-3")
  {
    canonicalUnit = "g/cm3";
    factor = 1.0;
    return true;
  }
  if (u == "kg/m3" || u == "kg/m^3")
  {
    canonicalUnit = "g/cm3";
    factor = 1e-3;
    return true;
  }
  // Cross-section (canonical barn)
  if (u == "barn" || u == "b")
  {
    canonicalUnit = "barn";
    factor = 1.0;
    return true;
  }
  if (u == "mb")
  {
    canonicalUnit = "barn";
    factor = 1e-3;
    return true;
  }
  if (u == "ub")
  {
    canonicalUnit = "barn";
    factor = 1e-6;
    return true;
  }
  if (u == "cm2" || u == "cm^2")
  {
    canonicalUnit = "barn";
    factor = 1e24;
    return true;
  }
  return false;  // unknown -> never guessed
}

G4AgentMeasurement G4AgentAnalysis::Measurement(const G4String& value, const G4String& uncertainty,
                                                const G4String& unit)
{
  G4AgentMeasurement m;
  m.unit = Trim(unit);
  const G4String vtext = Trim(value);
  if (!vtext.empty())
  {
    try
    {
      std::size_t pos = 0;
      m.value = std::stod(std::string{vtext}, &pos);
      // Reject trailing junk ("5 events" is not a bare number).
      const G4String tail = Trim(G4String(vtext.substr(pos)));
      m.valid = tail.empty();
    }
    catch (...)
    {
      m.valid = false;
    }
  }
  if (m.valid)
  {
    const G4String utext = Trim(uncertainty);
    if (!utext.empty())
    {
      try
      {
        const double u = std::stod(std::string{utext});
        if (u > 0.0 && std::isfinite(u))
        {
          m.uncertainty = u;
          m.hasUncertainty = true;
        }
      }
      catch (...)
      {}
    }
    m.unitKnown = NormalizeUnit(m.unit, m.canonicalUnit, m.toCanonical);
  }
  return m;
}

G4String G4AgentAnalysis::Compare(const G4String& observable, const G4AgentMeasurement& simIn,
                                  const G4AgentMeasurement& refIn, double thresholdSigma)
{
  G4AgentReport report;
  report.AddLine("observable : " + observable);

  auto describe = [](const G4AgentMeasurement& m) {
    if (!m.valid) return G4String("NOT AVAILABLE");
    G4String s = Num(m.value);
    if (m.hasUncertainty) s += " +/- " + Num(m.uncertainty);
    if (!m.unit.empty()) s += " " + m.unit;
    return s;
  };
  report.AddLine("sim        : " + describe(simIn));
  report.AddLine("reference  : " + describe(refIn));

  if (!simIn.valid || !refIn.valid)
  {
    report.AddLine("verdict    : " + VerdictName(G4AgentVerdict::NotComparable));
    report.AddLine("rule       : missing numeric value on one side");
    return report.AsString();
  }
  if (!simIn.unitKnown || !refIn.unitKnown)
  {
    report.AddLine("verdict    : " + VerdictName(G4AgentVerdict::NotComparable));
    report.AddLine("rule       : unknown unit ('" + simIn.unit + "' vs '" + refIn.unit + "')");
    return report.AsString();
  }
  if (simIn.canonicalUnit != refIn.canonicalUnit)
  {
    report.AddLine("verdict    : " + VerdictName(G4AgentVerdict::NotComparable));
    report.AddLine("rule       : incompatible units (" + simIn.canonicalUnit + " vs "
                   + refIn.canonicalUnit + ")");
    return report.AsString();
  }

  // Display helper: canonical "1" is dimensionless; omit it from value
  // suffixes but name it in the unit-check line.
  auto canonName = [](const G4String& u) {
    return u == "1" ? G4String("dimensionless") : u;
  };
  auto suffix = [](const G4String& u) {
    return u == "1" ? G4String("") : " " + u;
  };

  const double sim = simIn.value * simIn.toCanonical;
  const double ref = refIn.value * refIn.toCanonical;
  const double diff = sim - ref;
  report.AddLine("unit check : compatible (" + canonName(simIn.canonicalUnit)
                 + ", values normalized)");
  report.AddLine("difference : " + Num(diff) + suffix(simIn.canonicalUnit));

  if (!simIn.hasUncertainty || !refIn.hasUncertainty)
  {
    report.AddLine("verdict    : " + VerdictName(G4AgentVerdict::Inconclusive));
    report.AddLine(
      "rule       : missing or zero uncertainty on one side; difference shown but "
      "no consistency test");
    return report.AsString();
  }

  const double simUnc = simIn.uncertainty * simIn.toCanonical;
  const double refUnc = refIn.uncertainty * refIn.toCanonical;
  const double denom = std::sqrt(simUnc * simUnc + refUnc * refUnc);
  const double pull = diff / denom;
  const G4AgentVerdict verdict =
    (std::fabs(pull) <= thresholdSigma) ? G4AgentVerdict::Consistent : G4AgentVerdict::Contrary;
  report.AddLine("sim sigma  : " + Num(simUnc) + suffix(simIn.canonicalUnit));
  report.AddLine("ref sigma  : " + Num(refUnc) + suffix(simIn.canonicalUnit));
  report.AddLine("pull       : " + Num(pull));
  report.AddLine("threshold  : " + Num(thresholdSigma) + " sigma");
  report.AddLine("verdict    : " + VerdictName(verdict));
  report.AddLine("rule       : |pull| <= threshold -> CONSISTENT else CONTRARY");
  return report.AsString();
}

G4String G4AgentAnalysis::Run(const G4String& subcommand, const G4String& argument) const
{
  const G4String sub = ToLower(Trim(subcommand));
  if (sub.empty() || sub == "status") return Status();
  if (sub == "analyze") return Analyze(Trim(argument));
  if (sub == "compare") return CompareObservables(Trim(argument));
  G4AgentReport report;
  report.SetHeader("[G4AGENT] analysis");
  report.AddLine("error : unknown subcommand '" + subcommand + "'");
  report.AddLine("usage : analyze <run-id> | compare <run-id> | status");
  return report.AsString();
}

G4String G4AgentAnalysis::Status() const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] analysis status");

  auto fileState = [&](const G4String& rel, const char* label) {
    G4String abs;
    G4String state = "NOT CONFIGURED";
    if (fProject.Usable() && fProject.ResolveInside(rel, abs))
    {
      std::error_code ec;
      if (G4fs::is_regular_file(P(abs), ec) && !ec) state = "present";
    }
    report.AddLine(G4String(label) + " : " + state);
  };
  report.AddLine("project : " + (fProject.Usable() ? fProject.Root() : G4String("unavailable")));
  fileState("validation/observables.txt", "observables");
  fileState("validation/references.txt", "references");
  report.AddLine("verdicts : CONSISTENT | CONTRARY | INCONCLUSIVE | NOT COMPARABLE");
  report.AddLine("scope    : deterministic numeric comparison; never scientific acceptance");
  return report.AsString();
}

G4bool G4AgentAnalysis::Extract(const G4String& runId, std::vector<Extracted>& out,
                                G4String& reason) const
{
  out.clear();
  if (!fProject.Usable())
  {
    reason = "project root unavailable";
    return false;
  }
  if (runId.empty())
  {
    reason = "run id required";
    return false;
  }

  G4String specAbs;
  if (!fProject.ResolveInside("validation/observables.txt", specAbs))
  {
    reason = "observable spec outside project root";
    return false;
  }
  std::string specText;
  {
    std::error_code ec;
    if (!G4fs::is_regular_file(P(specAbs), ec) || ec || !ReadWholeFile(specAbs, specText))
    {
      reason = "validation/observables.txt not configured";
      return false;
    }
  }

  G4String logAbs;
  if (!fProject.ResolveInside("runs/" + runId + "/log.txt", logAbs))
  {
    reason = "run id outside project root";
    return false;
  }
  std::string logText;
  {
    std::error_code ec;
    if (!G4fs::is_regular_file(P(logAbs), ec) || ec || !ReadWholeFile(logAbs, logText))
    {
      reason = "run '" + runId + "' log not found";
      return false;
    }
  }

  std::vector<std::string> lines;
  {
    std::istringstream in(logText);
    std::string l;
    while (std::getline(in, l))
      lines.push_back(l);
  }

  for (const auto& block : ParseBlocks(specText))
  {
    Extracted e;
    e.name = Lookup(block, "observable");
    if (e.name.empty()) continue;
    const G4String pattern = Lookup(block, "pattern");
    const G4String unit = Lookup(block, "unit");
    e.source = "not available";
    if (!pattern.empty())
    {
      try
      {
        const std::regex re(std::string{pattern});
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
          std::smatch m;
          if (std::regex_search(lines[i], m, re))
          {
            // Group 1 = value, group 2 (if present) = uncertainty; otherwise
            // a literal `uncertainty` field may supply it. Never inferred.
            G4String captured;
            G4String unc;
            if (m.size() > 2)
            {
              captured = G4String(m[1].str());
              unc = G4String(m[2].str());
            }
            else if (m.size() > 1)
            {
              captured = G4String(m[1].str());
            }
            else
            {
              captured = G4String(m[0].str());
            }
            if (unc.empty()) unc = Lookup(block, "uncertainty");
            e.measurement = Measurement(captured, unc, unit);
            e.source = "runs/" + runId + "/log.txt:" + std::to_string(i + 1);
            break;
          }
        }
      }
      catch (const std::regex_error&)
      {
        e.source = "pattern error in validation/observables.txt";
      }
    }
    if (e.source == "not available")
      e.measurement = Measurement("", "", unit);  // invalid, unit still normalized
    out.push_back(e);
  }
  if (out.empty())
  {
    reason = "no observables declared in validation/observables.txt";
    return false;
  }
  return true;
}

G4bool G4AgentAnalysis::LoadReferences(std::vector<Extracted>& out, G4String& reason) const
{
  out.clear();
  if (!fProject.Usable())
  {
    reason = "project root unavailable";
    return false;
  }
  G4String abs;
  if (!fProject.ResolveInside("validation/references.txt", abs))
  {
    reason = "references path outside project root";
    return false;
  }
  std::string text;
  {
    std::error_code ec;
    if (!G4fs::is_regular_file(P(abs), ec) || ec || !ReadWholeFile(abs, text))
    {
      reason = "validation/references.txt not configured";
      return false;
    }
  }
  for (const auto& block : ParseBlocks(text))
  {
    Extracted e;
    e.name = Lookup(block, "observable");
    if (e.name.empty()) continue;
    e.measurement =
      Measurement(Lookup(block, "value"), Lookup(block, "uncertainty"), Lookup(block, "unit"));
    const G4String src = Lookup(block, "source");
    e.source = src.empty() ? G4String("validation/references.txt") : src;
    out.push_back(e);
  }
  if (out.empty())
  {
    reason = "no references declared in validation/references.txt";
    return false;
  }
  return true;
}

G4String G4AgentAnalysis::Analyze(const G4String& runId) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] analyze run");

  std::vector<Extracted> obs;
  G4String reason;
  if (!Extract(runId, obs, reason))
  {
    report.AddLine("error  : " + reason);
    return report.AsString();
  }

  report.AddLine("run    : " + runId);
  report.AddLine("count  : " + std::to_string(obs.size()));
  for (const auto& e : obs)
  {
    report.AddLine("observable : " + e.name);
    if (e.measurement.valid)
    {
      G4String v = Num(e.measurement.value);
      if (!e.measurement.unit.empty()) v += " " + e.measurement.unit;
      report.AddLine("value      : " + v);
    }
    else
    {
      report.AddLine("value      : NOT AVAILABLE");
    }
    report.AddLine("source     : " + e.source);
  }
  report.AddLine(
    "note   : values are extracted verbatim from the run log by the declared "
    "patterns; unmatched patterns are NOT AVAILABLE, never guessed");
  return report.AsString();
}

G4String G4AgentAnalysis::CompareObservables(const G4String& runId) const
{
  G4AgentReport report;
  report.SetHeader("[G4AGENT] compare run");

  std::vector<Extracted> obs;
  G4String reason;
  if (!Extract(runId, obs, reason))
  {
    report.AddLine("error : " + reason);
    return report.AsString();
  }
  std::vector<Extracted> refs;
  if (!LoadReferences(refs, reason))
  {
    report.AddLine("error : " + reason);
    return report.AsString();
  }

  report.AddLine("run     : " + runId);
  const double threshold = 2.0;
  int compared = 0;
  int consistent = 0;

  for (const auto& ref : refs)
  {
    const Extracted* match = nullptr;
    for (const auto& o : obs)
    {
      if (o.name == ref.name)
      {
        match = &o;
        break;
      }
    }
    report.AddLine("");
    if (match == nullptr)
    {
      report.AddLine("observable : " + ref.name);
      report.AddLine("sim        : NOT AVAILABLE");
      report.AddLine("reference  : "
                     + (ref.measurement.valid ? Num(ref.measurement.value) : "NOT AVAILABLE"));
      report.AddLine("verdict    : " + VerdictName(G4AgentVerdict::NotComparable));
      report.AddLine("rule       : observable not extracted from the run");
      continue;
    }
    const G4String block = Compare(ref.name, match->measurement, ref.measurement, threshold);
    std::istringstream in(std::string{block});
    std::string line;
    while (std::getline(in, line))
      report.AddLine(line);
    report.AddLine("source     : " + ref.source);
    ++compared;
    if (block.find("verdict    : CONSISTENT") != G4String::npos) ++consistent;
  }

  report.AddLine("");
  report.AddLine("compared   : " + std::to_string(compared));
  report.AddLine("consistent : " + std::to_string(consistent));
  if (compared == 0)
    report.AddLine("overall    : INSUFFICIENT EVIDENCE (no observable compared)");
  else
    report.AddLine("overall    : numeric comparison over extracted values only");
  report.AddLine(
    "scope      : mechanical numeric comparison of simulator output against "
    "operator-supplied references; NOT scientific acceptance, and no verdict is "
    "drawn from a model opinion");
  return report.AsString();
}
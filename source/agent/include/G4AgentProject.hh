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
// G4AgentProject
//
// Class description
//
// Read-only project inspection tools: list, read,
// search and VCS-status inspection for a single confined project root
// (G4_AGENT_PROJECT_DIR, default: current working directory). These are
// deliberately separate from the Geant4 runtime services (G4AgentInspect/
// G4AgentCheck): they know nothing about the simulation and the runtime
// path never depends on them .
//
// Every operation is strictly read-only and path-confined: arguments are
// resolved lexically against the root, '..' escapes are refused, symlinked
// results outside the root are refused, output is size-capped, and no
// write, delete, move, or general shell capability exists. The only
// subprocess ever spawned is a fixed git command for the diff view, with
// the already-confined target passed as a single-quoted pathspec argument;
// on non-POSIX platforms the git helper is not used at all. The class also
// derives deterministic default build/run commands from the project layout
// (CMake or GNU Make); executing them remains the caller's responsibility.

#ifndef G4AGENTPROJECT_HH
#define G4AGENTPROJECT_HH

#include "G4String.hh"

class G4AgentProject
{
  public:

    explicit G4AgentProject(const G4String& rootPath);

    // Root from G4_AGENT_PROJECT_DIR; when unset, the current working
    // directory is used.
    static G4AgentProject FromEnvironment();

    // Raw G4_AGENT_PROJECT_DIR value (empty when unset), for reporting.
    static G4String ConfiguredRoot();

    // Raw G4_AGENT_BUILD_COMMAND value (operator-configured, fixed; the
    // model never contributes arguments). Empty when unset.
    static G4String ConfiguredBuildCommand();

    // Raw G4_AGENT_RUN_COMMAND value (operator-configured fixed prefix for
    // executing the project application; a confined macro path is appended
    // as a single argument by G4AgentRuns). Empty when unset.
    static G4String ConfiguredRunCommand();

    // Effective command with its provenance. `text` is empty when no
    // command is available; `reason` then explains why. Detection is
    // deterministic and confined to the project root:
    //
    //   build: a valid CMake build tree -> cmake --build
    //          CMakeLists.txt without a tree -> cmake -S . -B build && build
    //          GNUmakefile only -> make -j
    //   run:   the unique add_executable target (or a directory-name match
    //          when several exist) -> ./build/<target>
    //          in-source CMake build -> ./<target>
    //          GNUmakefile `name :=` -> ./<target>
    //
    // G4_AGENT_BUILD_COMMAND / G4_AGENT_RUN_COMMAND always take precedence.
    struct Command
    {
        G4String text;
        G4String source;
        G4String reason;
    };

    Command EffectiveBuildCommand() const;
    Command EffectiveRunCommand() const;

    // Escape single quotes for embedding inside a '...' shell word.
    static G4String SanitizeForShell(const G4String& text);

    const G4String& Root() const { return fRoot; }
    G4bool Usable() const { return fUsable; }

    // Blocks without the [G4AGENT] header; assembled by the tool layer.
    G4String ListFiles(const G4String& directory) const;
    G4String ReadFile(const G4String& path) const;
    G4String Search(const G4String& pattern, const G4String& directory) const;
    G4String Diff(const G4String& target) const;

    // Actuating project operation (EDIT-gated by the caller):
    // create or replace one confined text file. Refuses binary content and
    // the journal file itself (journal integrity).
    G4String WriteFile(const G4String& path, const G4String& content) const;

    // Append a journal block to
    // <root>/G4AGENT_CHANGES.md under the project root. Best effort.
    void JournalAppend(const G4String& block) const;

    // Lexical + symlink-confined resolution under the root. False when the
    // argument escapes or references outside the project root.
    G4bool ResolveInside(const G4String& path, G4String& resolved) const;

  private:

    G4String fRoot;  // normalized absolute (lexical), empty when unusable
    G4bool fUsable = false;
};

#endif

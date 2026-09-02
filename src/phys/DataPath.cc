#include <climits>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

#include "DataPath.hh"

#ifndef MUONSIM_DATADIR
#define MUONSIM_DATADIR ""
#endif

namespace
{
bool Exists(const std::string & p)
{
  struct stat st;
  return !p.empty() && stat(p.c_str(), &st) == 0;
}

// Directory holding the running executable, or empty if it cannot be determined.
std::string ExecutableDir()
{
  char buf[PATH_MAX];
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return "";
  buf[n] = '\0';
  std::string path(buf);
  const auto slash = path.rfind('/');
  return (slash == std::string::npos) ? "" : path.substr(0, slash);
}
} // namespace

std::string ResolveDataFile(const std::string & relative)
{
  if (Exists(relative)) return relative;

  if (const char * env = std::getenv("MUONSIM_DATA")) {
    const std::string p = std::string(env) + "/" + relative;
    if (Exists(p)) return p;
  }

  const std::string exeDir = ExecutableDir();
  if (!exeDir.empty()) {
    for (const char * up : {"/../data/", "/../../data/", "/data/"}) {
      const std::string p = exeDir + up + relative;
      if (Exists(p)) return p;
    }
  }

  const std::string compiled = MUONSIM_DATADIR;
  if (!compiled.empty()) {
    const std::string p = compiled + "/" + relative;
    if (Exists(p)) return p;
  }

  return ""; // not found -- the caller reports it, with the name it was looking for
}

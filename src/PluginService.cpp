/*****************************************************************************\
* (c) Copyright 2013 CERN                                                     *
*                                                                             *
* This software is distributed under the terms of the GNU General Public      *
* Licence version 3 (GPL Version 3), copied verbatim in the file "LICENCE".   *
*                                                                             *
* In applying this licence, CERN does not waive the privileges and immunities *
* granted to it by virtue of its status as an Intergovernmental Organization  *
* or submit itself to any jurisdiction.                                       *
\*****************************************************************************/

/// @author Marco Clemencic <marco.clemencic@cern.ch>

#include <Gaudi/PluginService.h>

#include <dlfcn.h>
#include <dirent.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>

#include <cxxabi.h>
#include <sys/stat.h>

// string trimming functions taken from
// http://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
#include <algorithm>
// trim from start
static inline std::string &ltrim(std::string &s) {
        s.erase(s.begin(),
                std::find_if(s.begin(), s.end(),
                             std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(),
                             std::not1(std::ptr_fun<int, int>(std::isspace)))
                                       .base(),
                s.end());
        return s;
}
// trim from both ends
static inline std::string &trim(std::string &s) {
        return ltrim(rtrim(s));
}

namespace {
  inline void factoryInfoSetHelper(std::string& dest, const std::string value,
                                   const std::string& desc,
                                   const std::string& id) {
    if (dest.empty()) {
      dest = value;
    } else if (dest != value) {
      std::ostringstream o;
      o << "new factory loaded for '" << id << "' with different "
        << desc << ": " << dest << " != " << value;
      Gaudi::PluginService::Details::logger().warning(o.str());
    }
  }
}

namespace Gaudi { namespace PluginService {

  Exception::Exception(const std::string& msg): m_msg(msg) {}
  Exception::~Exception() throw() {}
  const char*  Exception::what() const throw() {
    return m_msg.c_str();
  }

  namespace Details {
    void* getCreator(const std::string& id, const std::string& type) {
      return Registry::instance().get(id, type);
    }

    Registry& Registry::instance() {
      static Registry r;
      return r;
    }

    Registry::Registry(): m_initialized(false) {}

    void Registry::initialize() {
      m_initialized = true;
#ifdef WIN32
      const char* envVar = "PATH";
      const char sep = ';';
#else
      const char* envVar = "LD_LIBRARY_PATH";
      const char sep = ':';
#endif
      char *search_path = ::getenv(envVar);
      if (search_path) {
        logger().debug(std::string("searching factories in ") + envVar);
        std::string path(search_path);
        std::string::size_type pos = 0;
        std::string::size_type newpos = 0;
        while (pos != std::string::npos) {
          std::string dirName;
          // get the next entry in the path
          newpos = path.find(sep, pos);
          if (newpos != std::string::npos) {
            dirName = path.substr(pos, newpos - pos);
            pos = newpos+1;
          } else {
            dirName = path.substr(pos);
            pos = newpos;
          }
          logger().debug(std::string(" looking into ") + dirName);
          // look for files called "*.factories" in the directory
          DIR *dir = opendir(dirName.c_str());
          if (dir) {
            struct dirent * entry;
            while ((entry = readdir(dir))) {
              std::string name(entry->d_name);
              // check if the file name ends with ".components"
              std::string::size_type extpos = name.find(".components");
              if ((extpos != std::string::npos) &&
                  ((extpos+11) == name.size())) {
                std::string fullPath = (dirName + '/' + name);
                { // check if it is a regular file
                  struct stat buf;
                  stat(fullPath.c_str(), &buf);
                  if (!S_ISREG(buf.st_mode)) continue;
                }
                // read the file
                logger().debug(std::string("  reading ") + name);
                std::ifstream factories(fullPath.c_str());
                std::string line;
                int factoriesCount = 0;
                int lineCount = 0;
                while (!factories.eof()) {
                  ++lineCount;
                  std::getline(factories, line);
                  trim(line);
                  // skip empty lines and lines starting with '#'
                  if (line.empty() || line[0] == '#') continue;
                  // look for the separator
                  std::string::size_type pos = line.find(':');
                  if (pos == std::string::npos) {
                    std::ostringstream o;
                    o << "failed to parse line " << fullPath
                      << ':' << lineCount;
                    logger().warning(o.str());
                    continue;
                  }
                  const std::string lib(line, 0, pos);
                  const std::string fact(line, pos+1);
                  m_factories.insert(std::make_pair(fact, FactoryInfo(lib)));
                  ++factoriesCount;
                }
                if (logger().level() <= Logger::Debug) {
                  std::ostringstream o;
                  o << "  found " << factoriesCount << " factories";
                  logger().debug(o.str());
                }
              }
            }
            closedir(dir);
          }
        }
      }
    }

    void Registry::add(const std::string& id, void *factory,
                       const std::string& type, const std::string& rtype,
                       const std::string& className){
      FactoryMap &facts = factories();
      FactoryMap::iterator entry = facts.find(id);
      if (entry == facts.end())
      {
        // this factory was not known yet
        facts.insert(std::make_pair(id, FactoryInfo("unknown", factory,
                                                    type, rtype, className)));
      } else {
        // do not replace an existing factory for a new one
        if (!entry->second.ptr) {
          entry->second.ptr = factory;
        }
        factoryInfoSetHelper(entry->second.type, type, "type", id);
        factoryInfoSetHelper(entry->second.rtype, rtype, "return type", id);
        factoryInfoSetHelper(entry->second.className, className, "class", id);
      }
    }

    void* Registry::get(const std::string& id, const std::string& type) const {
      const FactoryMap &facts = factories();
      FactoryMap::const_iterator f = facts.find(id);
      if (f != facts.end())
      {
        if (!f->second.ptr) {
          if (!dlopen(f->second.library.c_str(), RTLD_NOW | RTLD_LOCAL)) {
            logger().warning("cannot load " + f->second.library +
                             " for factory " + id);
            char *dlmsg = dlerror();
            if (dlmsg)
              logger().warning(dlmsg);
            return 0;
          }
          f = facts.find(id); // ensure that the iterator is valid
        }
        if (f->second.type == type)
          return f->second.ptr;
        else
          logger().warning("found factory " + id + ", but of wrong type: " +
                           f->second.type + " instead of " + type);
      }
      return 0; // factory not found
    }

    const Registry::FactoryInfo& Registry::getInfo(const std::string& id) const {
      static FactoryInfo unknown("unknown");
      const FactoryMap &facts = factories();
      FactoryMap::const_iterator f = facts.find(id);
      if (f != facts.end())
      {
        return f->second;
      }
      return unknown; // factory not found
    }

    std::set<Registry::KeyType> Registry::loadedFactories() const {
      std::set<KeyType> l;
      for (FactoryMap::const_iterator f = m_factories.begin();
           f != m_factories.end(); ++f)
      {
        if (f->second.ptr)
          l.insert(f->first);
      }
      return l;
    }

    std::string demangle(const std::type_info& id) {
      int   status;
      char* realname;
      realname = abi::__cxa_demangle(id.name(), 0, 0, &status);
      if (realname == 0) return id.name();
      std::string result(realname);
      free(realname);
      return result;
    }

    void Logger::report(Level lvl, const std::string& msg) {
      static const char* levels[] = {"DEBUG  : ",
                                     "INFO   : ",
                                     "WARNING: ",
                                     "ERROR  : "};
      if (lvl >= level()) {
        std::cerr << levels[lvl] << msg << std::endl;
      }
    }

    static std::auto_ptr<Logger> s_logger(new Logger);
    Logger& logger() {
      return *s_logger;
    }
    void setLogger(Logger* logger) {
      s_logger.reset(logger);
    }

  } // namespace Details

  void SetDebug(int debugLevel) {
    using namespace Details;
    Logger& l = logger();
    if (debugLevel > 1)
      l.setLevel(Logger::Debug);
    else if (debugLevel > 0)
      l.setLevel(Logger::Info);
    else l.setLevel(Logger::Warning);
  }

  int Debug() {
    using namespace Details;
    switch (logger().level()) {
    case Logger::Debug: return 2; break;
    case Logger::Info: return 1; break;
    default: return 0;
    }
  }

}} // namespace Gaudi::PluginService

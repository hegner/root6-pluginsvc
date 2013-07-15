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

#include <memory>
#include <iostream>

#include <Gaudi/PluginService.h>
#include "Interfaces.h"

void call(MyInterface *p) {
   std::cout << std::showbase << std::hex << std::nouppercase << (unsigned long)p << std::endl;
   if (p) p->theMethod();
}

int main(int argc, char ** argv)
{

  std::string A("A");
  std::string B("B");

  std::cout << "Looking for classes..." << std::endl;
  std::auto_ptr<MyInterface> c1a( MyFactory::create("Class1", "c1a", &A) );
  std::auto_ptr<MyInterface> c2a( MyFactory::create("Class2", "c2a", &B) );
  call(c1a.get());
  call(c2a.get());

  std::cout << "Looking for IDs..." << std::endl;
  std::auto_ptr<MyInterface> c1b( MyFactory::create("1", "c1b", &B) );
  std::auto_ptr<MyInterface> c2b( MyFactory::create("2", "c2b", &A) );
  call(c1b.get());
  call(c2b.get());

  try {
    MyFactory::create("3", "c1b", &B);
  } catch (Gaudi::PluginService::Exception &e) {
    std::cout << "PluginService::Exception -> " << e.what() << std::endl;
  }

  std::cout << "Done!" << std::endl;
}

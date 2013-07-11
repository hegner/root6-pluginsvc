// cl -nologo -Z7 -MDd -GR -EHsc testLoading.cxx -I %ROOTSYS%\include /link -debug -LIBPATH:%ROOTSYS%\lib libCore.lib

#include "TROOT.h"
#include "Riostream.h"
#include "TH1.h"
#include <memory>

#include "PluginFactory.h"
#include "Interfaces.h"

void call(MyInterface *p) {
   std::cout << std::showbase << std::hex << std::nouppercase << (unsigned long)p << std::endl;
   if (p) p->theMethod();
}

int main(int argc, char ** argv)
{
   std::cout << "Looking for classes..." << std::endl;
   std::auto_ptr<MyInterface> c1a( CALL1(MyInterface*, Class1, std::string("c1a")) );
   std::auto_ptr<MyInterface> c2a( CALL1(MyInterface*, Class2, std::string("c2a")) );
   call(c1a.get());
   call(c2a.get());

   std::cout << "Looking for IDs..." << std::endl;
   std::auto_ptr<MyInterface> c1b( CALL1(MyInterface*, 1, std::string("c1b")) );
   std::auto_ptr<MyInterface> c2b( CALL1(MyInterface*, 2, std::string("c2b")) );
   call(c1b.get());
   call(c2b.get());
   std::cout << "Done!" << std::endl;
}
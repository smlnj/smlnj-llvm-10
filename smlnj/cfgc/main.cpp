/// \file main.cpp
///
/// \copyright 2024 The Fellowship of SML/NJ (http://www.smlnj.org)
/// All rights reserved.
///
/// \brief Main test driver for the code generator
///
/// \author John Reppy
///

#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/Value.h"

#include "cfg.hpp"
#include "context.hpp"
#include "target-info.hpp"

#if defined(ARCH_AMD64)
#define HOST_ARCH "x86_64"
#elif defined(ARCH_ARM64)
#define HOST_ARCH "aarch64"
#else
#  error unknown architeture
#endif

/// different output targets
enum class output { PrintAsm, AsmFile, ObjFile, Memory };

// set the target architecture.  This call returns `true` when there
// is an error and `false` otherwise.
//
bool setTarget (std::string const &target);

// generate code
void codegen (std::string const & src, bool emitLLVM, bool dumpBits, output out);

extern "C" {
void Die (const char *fmt, ...)
{
    va_list	ap;

    va_start (ap, fmt);
    fprintf (stderr, "cfgc: Fatal error -- ");
    vfprintf (stderr, fmt, ap);
    fprintf (stderr, "\n");
    va_end(ap);

    ::exit (1);
}
} // extern "C"

[[noreturn]] void usage ()
{
    std::cerr << "usage: cfgc [ -o | -S | -c ] [ --emit-llvm ] [ --bits ] [ --target <target> ] <pkl-file>\n";
    std::cerr << "options:\n";
    std::cerr << "    -o                -- generate an object file\n";
    std::cerr << "    -S                -- emit target assembly code to a file\n";
    std::cerr << "    -c                -- use JIT compiler and loader to produce code object\n";
    std::cerr << "    -emit-llvm        -- emit generated LLVM assembly to standard output\n";
    std::cerr << "    -bits             -- output the code-object bits (implies \"-c\" flag)\n";
    std::cerr << "    -target <target>  -- specify the target architecture (default "
              << HOST_ARCH << ")\n";
    exit (1);
}

int main (int argc, char **argv)
{
    output out = output::PrintAsm;
    bool emitLLVM = false;
    bool dumpBits = false;
    std::string src = "";
#if defined(ARCH_AMD64)
    std::string targetArch = "x86_64";
#elif defined(ARCH_ARM64)
    std::string targetArch = "aarch64";
#else
#  error unknown architeture
#endif

    std::vector<std::string> args(argv+1, argv+argc);

    if (args.empty()) {
	usage();
    }

    for (int i = 0;  i < args.size();  i++) {
	if (args[i][0] == '-') {
	    if (args[i] == "-o") {
		out = output::ObjFile;
	    } else if (args[i] == "-S") {
		out = output::AsmFile;
	    } else if (args[i] == "-c") {
		out = output::Memory;
	    } else if (args[i] == "--emit-llvm") {
		emitLLVM = true;
	    } else if (args[i] == "--bits") {
		dumpBits = true;
		out = output::Memory;
	    } else if (args[i] == "--target") {
		i++;
		if (i < args.size()) {
		    targetArch = args[i];
		} else {
		    usage();
		}
	    } else {
		usage();
	    }
	}
	else if (i < args.size()-1) {
            usage();
	}
	else { // last argument
	    src = args[i];
	}
    }
    if (src.empty()) {
        usage();
    }

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    if (setTarget (targetArch)) {
	std::cerr << "codegen: unable to set target to \"" << targetArch << "\"\n";
	return 1;
    }

    codegen (src, emitLLVM, dumpBits, out);

    return 0;

}

/***** Code Generation *****/

//! points to a dynamically allocated code buffer; this pointer gets
//! reset if we change the target architecture.
//
static smlnj::cfgcg::Context *gContext = nullptr;

/// set the target machine
//
bool setTarget (std::string const &target)
{
    if (gContext != nullptr) {
	if (gContext->targetInfo()->name == target) {
	    return false;
	}
	delete gContext;
    }

    gContext = smlnj::cfgcg::Context::create (target);

    return (gContext == nullptr);

}

// timer support
#include <time.h>

class Timer {
  public:
    static Timer start ()
    {
	struct timespec ts;
	clock_gettime (CLOCK_REALTIME, &ts);
	return Timer (_cvtTimeSpec(ts));
    }
    void restart ()
    {
	struct timespec ts;
	clock_gettime (CLOCK_REALTIME, &ts);
	this->_ns100 = _cvtTimeSpec(ts);
    }
    double msec () const
    {
	struct timespec ts;
	clock_gettime (CLOCK_REALTIME, &ts);
	double t = double(_cvtTimeSpec(ts) - this->_ns100);
	return t / 10000.0;
    }
  private:
    uint64_t _ns100;	// track time in 100's of nanoseconds
    static uint64_t _cvtTimeSpec (struct timespec &ts)
    {
	return (
	    10000000 * static_cast<uint64_t>(ts.tv_sec)
	    + static_cast<uint64_t>(ts.tv_nsec) / 100);
    }
    Timer (uint64_t t) : _ns100(t) { }
};

void codegen (std::string const & src, bool emitLLVM, bool dumpBits, output out)
{
    assert (gContext != nullptr && "call setTarget before calling codegen");

    asdl::file_instream inS(src);

    std::cout << "read pickle ..." << std::flush;
    Timer unpklTimer = Timer::start();
    CFG::comp_unit *cu = CFG::comp_unit::read (inS);
    std::cout << " " << unpklTimer.msec() << "ms\n" << std::flush;

    // generate LLVM
    std::cout << " generate llvm ..." << std::flush;;
    Timer genTimer = Timer::start();
    cu->codegen (gContext);
    std::cout << " " << genTimer.msec() << "ms\n" << std::flush;

    if (emitLLVM) {
	gContext->dump ();
    }

    if (! gContext->verify ()) {
	std::cerr << "Module verified\n";
    }

    std::cout << " optimize ..." << std::flush;;
    Timer optTimer = Timer::start();
    gContext->optimize ();
    std::cout << " " << optTimer.msec() << "ms\n" << std::flush;

//    if (emitLLVM) {
//	gContext->dump ();
//    }

    if (! gContext->verify ()) {
	std::cerr << "Module verified after optimization\n";
    }

    // get the stem of the filename
    std::string stem(src);
    auto pos = stem.rfind(".pkl");
    if (pos+4 != stem.size()) {
	stem = "out";
    }
    else {
	stem = stem.substr(0, pos);
    }

    switch (out) {
      case output::PrintAsm:
	gContext->dumpAsm();
	break;
      case output::AsmFile:
	gContext->dumpAsm (stem);
	break;
      case output::ObjFile:
	gContext->dumpObj (stem);
	break;
      case output::Memory: {
	    auto obj = gContext->compile ();
	    if (obj) {
		obj->dump(dumpBits);
	    }
	} break;
    }

    gContext->endModule();

} /* codegen */

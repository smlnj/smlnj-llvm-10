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

///
#define TIME_CODEGEN 1
#define VERIFY_LLVM 1

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
    exit (1);
}

int main (int argc, char **argv)
{
    output out = output::PrintAsm;
    bool emitLLVM = false;
    bool dumpBits = false;
    std::string src = "";
    std::string targetArch = HOST_ARCH;

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
static code_buffer *CodeBuf = nullptr;

/// set the target machine
//
bool setTarget (std::string const &target)
{
    if (CodeBuf != nullptr) {
	if (CodeBuf->targetInfo()->name == target) {
	    return false;
	}
	delete CodeBuf;
    }

    CodeBuf = code_buffer::create (target);

    return (CodeBuf == nullptr);

}

#if defined(TIME_CODEGEN)
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
#endif // defined(TIME_CODEGEN)

void codegen (std::string const & src, bool emitLLVM, bool dumpBits, output out)
{
#if defined(TIME_CODEGEN)
    Timer totalTimer = Timer::start();
#endif // defined(TIME_CODEGEN)

    assert (CodeBuf != nullptr && "call setTarget before calling codegen");

    asdl::file_instream inS(src);

#if defined(TIME_CODEGEN)
    std::cout << "read pickle ..." << std::flush;
    Timer unpklTimer = Timer::start();
#endif // defined(TIME_CODEGEN)
    CFG::comp_unit *cu = CFG::comp_unit::read (inS);
#if defined(TIME_CODEGEN)
    std::cout << " " << unpklTimer.msec() << "ms\n" << std::flush;
#endif // defined(TIME_CODEGEN)

  // generate LLVM
#if defined(TIME_CODEGEN)
    std::cout << " generate llvm ..." << std::flush;;
    Timer genTimer = Timer::start();
#endif // defined(TIME_CODEGEN)
    cu->codegen (CodeBuf);
#if defined(TIME_CODEGEN)
    std::cout << " " << genTimer.msec() << "ms\n" << std::flush;
#endif // defined(TIME_CODEGEN)

    if (emitLLVM) {
	CodeBuf->dump ();
    }

#ifdef VERIFY_LLVM
#if defined(TIME_CODEGEN)
    Timer verifyTimer = Timer::start();
#endif // defined(TIME_CODEGEN)
    if (! CodeBuf->verify ()) {
	std::cerr << "Module verified\n";
    }
#if defined(TIME_CODEGEN)
    double verifyT = verifyTimer.msec();
#endif // defined(TIME_CODEGEN)
#else
#if defined(TIME_CODEGEN)
    double verifyT = 0.0;
#endif // defined(TIME_CODEGEN)
#endif

#if defined(TIME_CODEGEN)
    std::cout << " optimize ..." << std::flush;;
    Timer optTimer = Timer::start();
#endif // defined(TIME_CODEGEN)
    CodeBuf->optimize ();
#if defined(TIME_CODEGEN)
    std::cout << " " << optTimer.msec() << "ms\n" << std::flush;
#endif // defined(TIME_CODEGEN)

//    if (emitLLVM) {
//	CodeBuf->dump ();
//    }

#ifdef VERIFY_LLVM
#if defined(TIME_CODEGEN)
    Timer verifyTimer = Timer::start();
#endif // defined(TIME_CODEGEN)
    if (! CodeBuf->verify ()) {
	std::cerr << "Module verified after optimization\n";
    }
#if defined(TIME_CODEGEN)
    double verifyT = verifyTimer.msec();
#endif // defined(TIME_CODEGEN)
#else
#if defined(TIME_CODEGEN)
    double verifyT = 0.0;
#endif // defined(TIME_CODEGEN)
#endif

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
	CodeBuf->dumpAsm();
	break;
      case output::AsmFile:
	CodeBuf->dumpAsm (stem);
	break;
      case output::ObjFile:
	CodeBuf->dumpObj (stem);
	break;
      case output::Memory: {
	    auto obj = CodeBuf->compile ();
	    if (obj) {
		obj->dump(dumpBits);
	    }
	} break;
    }

    CodeBuf->endModule();

} /* codegen */

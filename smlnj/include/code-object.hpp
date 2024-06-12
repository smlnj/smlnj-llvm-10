/*! \file code-object.hpp
 *
 * An abstract interface to mediate between the object files generated by
 * LLVM and the SML/NJ in-memory code objects.
 *
 * \author John Reppy
 */

/*
 * COPYRIGHT (c) 2021 The Fellowship of SML/NJ (https://smlnj.org)
 * All rights reserved.
 */

#ifndef _CODE_OBJECT_HPP_
#define _CODE_OBJECT_HPP_

#include <vector>
#include "llvm/Object/ObjectFile.h"

struct TargetInfo;

//==============================================================================

/// \brief A representation of a relocation record, where we have normalized
/// the information based on the conventions for the object-file format
/// being used.
///
/// When patching the generated object code, we need to take both the object-file
/// format (OFF) and architecture into account.  This type is used to abstract over
/// the OFF into account.  The architecture-specific stuff is in the `CodeObject`
/// subclasses.
//
struct Relocation {
/* TODO: replace constructor with factory
    std::option<Relocation> create (Section &sect, llvm::object::RelocationRef &rr);
 * that returns NONE when the symbol is not defined.
 */

    /// \brief constructor
    /// \param sect  information about the section that the record applies to
    /// \param rr    the LLVM relocation-record reference
    Relocation (struct Section const &sect, llvm::object::RelocationRef const &rr);

    uint64_t type;      ///< the type of relocation record (OFF and architecture
                        ///  specific)
    uint64_t addr;      ///< the address of the relocation relative to the start
                        ///  of the object file.  This offset accounts for the
                        ///  start of the section w.r.t. the start of the code
                        ///  object.
    int64_t value;      ///< the computed value of the relocation

}; // struct Relocation

//==============================================================================

/// information about a section to be included in the heap-allocated
/// code object
//
class Section {
public:
    /// constructor
    Section (class CodeObject *objF, llvm::object::SectionRef &s, uint64_t off)
    : _objFile(objF), _sect(s), _separateRelocSec(false), _offset(off)
    { }

    void setRelocationSection (llvm::object::SectionRef &r)
    {
        assert (! this->_separateRelocSec && "multiple relocation sections");
        this->_separateRelocSec = true;
        this->_reloc = r;
    }

    const llvm::object::SectionRef &sectionRef () const { return this->_sect; }

    llvm::StringRef getName () const
    {
        auto name = this->_sect.getName ();
        if (name.takeError()) {
            return "<unknown section>";
        } else {
            return *name;
        }
    }
    bool isText () const { return this->_sect.isText(); }
    bool isData () const { return this->_sect.isData(); }
    uint64_t getAddress () const { return this->_sect.getAddress (); }
    uint64_t getAlignment () const { return this->_sect.getAlignment (); }
    uint64_t getIndex () const { return this->_sect.getIndex (); }
    uint64_t getSize () const { return this->_sect.getSize (); }
    llvm::Expected<llvm::StringRef> getContents () const
    {
        return this->_sect.getContents ();
    }

    llvm::iterator_range<llvm::object::relocation_iterator> relocations () const
    {
        if (this->_separateRelocSec) {
            return this->_reloc.relocations ();
        } else {
            return this->_sect.relocations ();
        }
    }

    CodeObject *codeObject () const { return this->_objFile; }

    uint64_t offset () const { return this->_offset; }

    bool isSection (llvm::object::SectionRef const &other)
    {
        return this->_sect == other;
    }

private:
    class CodeObject *_objFile;     ///< the owning object file
    llvm::object::SectionRef _sect; ///< the included section
/* FIXME: once we switch to C++17, we can use a std::optional<SectionRef> */
    bool _separateRelocSec;         ///< true if the relocation info for
                                    ///  `sect` is in a separate section
    llvm::object::SectionRef _reloc;///< a separate section containing the
                                    ///  relocation info for `sect`
    uint64_t _offset;               ///< offset of this section in the
                                    ///  code object (set by _computeSizes)

}; // struct Section

//==============================================================================

/// a code-object is container for the parts of an object file that are needed to
/// create the SML code object in the heap.  Its purpose is to abstract from
/// target architecture and object-file format dependencies.  This class is
/// an abstract base class; the actual implementation is specialized to the
/// target.
//
class CodeObject {
public:

    CodeObject () = delete;
    CodeObject (CodeObject &) = delete;

    virtual ~CodeObject ();

    /// create a code object.
    static std::unique_ptr<CodeObject> create (class code_buffer *codeBuf);

    /// return the size of the code in bytes
    size_t size() const { return this->_szb; }

    /// \brief copy the code into the given memory buffer while applying the
    ///        relocation patches.
    /// \param code  points to the destination address for the code; this memory
    ///              is assumed to be at least this->size() bytes.
    void getCode (unsigned char *code);

    /// dump information about the code object to the LLVM debug stream.
    void dump (bool bits);

    /// find a section by name
    /// \param name  the name of the section that we are searching for
    /// \return a pointer to the Section object or nullptr
    Section *findSection (llvm::StringRef name)
    {
        if ((this->_last != nullptr) && (this->_last->getName().equals(name))) {
            return this->_last;
        } else {
            for (int i = 0;  i < this->_sects.size();  ++i) {
                if (this->_sects[i].getName().equals(name)) {
                    this->_last = this->_sects.data() + i;
                    return this->_last;
                }
            }
            return nullptr;
        }
    }

    /// iterator over the symbols in the object file
    llvm::object::ObjectFile::symbol_iterator_range symbols () const
    {
        return this->_obj->symbols();
    }

    const TargetInfo *target () const { return this->_tgt; }

  protected:
    const TargetInfo *_tgt;
    std::unique_ptr<llvm::object::ObjectFile> _obj;

    /// the size of the heap-allocated code object in bytes
    size_t _szb;

    /// a vector of the sections that are to be included in the heap-allocated code
    /// object.
    std::vector<Section> _sects;
    Section *_last;                     ///< cache of last result returned by the
                                        ///  `findSection` method.

    /// constuctor
    CodeObject (
	const TargetInfo *target,
	std::unique_ptr<llvm::object::ObjectFile> objFile
    ) : _tgt(target), _obj(std::move(objFile)), _szb(0), _last(nullptr)
    { }

    /// helper function that determines which sections to include and computes
    /// the total size of the SML code object
    // NOTE: because this function invokes the target-specific virtual method
    // `_includeDataSect`, it must be called *after* the object has been
    // constructed.
    //
    void _computeSize ();

    /// should a section be included in the SML data object?
    //
    bool _includeSect (llvm::object::SectionRef const &sect)
    {
	return sect.isText() || (sect.isData() && this->_includeDataSect(sect));
    }

    /// check if a section contains relocation info for an included section?
    /// \param sect  the section being checked
    /// \return an iterator that references the section or `section_end()` for
    ///         the object file
    //
    llvm::object::section_iterator _relocationSect (llvm::object::SectionRef const &sect)
    {
        auto reloc = sect.getRelocatedSection();
        if (reloc
        && (*reloc != this->_obj->section_end())
        && this->_includeSect(**reloc)) {
            return *reloc;
        }
        else {
            return this->_obj->section_end();
        }
    }

    /// should a data section be included in the code object?  This method
    /// is target specific.
    //
    virtual bool _includeDataSect (llvm::object::SectionRef const &sect) = 0;

    /// helper function for resolving relocation records
    //
    virtual void _resolveRelocsForSection (Section const &sect, uint8_t *code) = 0;

    /// dump the relocation info for a section
    //
    void _dumpRelocs (llvm::object::SectionRef const &sect);

    /// convert relocation types to strings
    virtual std::string _relocTypeToString (uint64_t ty) = 0;

}; // CodeObject

#endif /// _CODE_OBJECT_HPP_
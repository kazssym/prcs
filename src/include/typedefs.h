/* -*-Mode: C++;-*-
 * PRCS - The Project Revision Control System
 * Copyright (C) 1997  Josh MacDonald
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * $Id$
 */


#ifndef _TYPEDEFS_H_
#define _TYPEDEFS_H_

enum AttrType {
  RealFileAttr,
  NoKeywordAttr,
  DirectoryAttr,
  SymlinkAttr,
  ImplicitDirectoryAttr,
  ProjectFileAttr,
  TagAttr,
  MergetoolAttr,
  DifftoolAttr,
  Mergerule1Attr,
  Mergerule2Attr,
  Mergerule3Attr,
  Mergerule4Attr,
  Mergerule5Attr,
  Mergerule6Attr,
  Mergerule7Attr,
  Mergerule8Attr,
  Mergerule9Attr,
  Mergerule10Attr,
  Mergerule11Attr,
  Mergerule12Attr,
  Mergerule13Attr,
  Mergerule14Attr
};

enum EstringType {
    EsStringLiteral,
    EsNameLiteral,
    EsUnProtected
};

enum SetkeysAction {
    Setkeys,
    Unsetkeys
};

enum ProjectReadData {
    KeepNothing            = 0,
    KeepMergeParents       = 1 << 2
};

enum ErrorToken {
    NonFatalError           = 1 << 0,
    FatalError              = 1 << 1,
    InternalError           = 1 << 2,
    InternalRepositoryError = 1 << 3,
    RepositoryError         = 1 << 4,
    WriteFailure            = 1 << 5,
    ReadFailure             = 1 << 6,
    StatFailure             = 1 << 7,
    UserAbort               = 1 << 8
};

enum MissingFileAction {
    QueryUserRemoveFromCommandLine,
    NoQueryUserRemoveFromCommandLine,
    SetNotPresentWarnUser
};

enum OverwriteStatus { DoesntExist, IgnoreMe, SameType };

enum NonErrorToken { NoError };

enum PrcsExitStatus {
    ExitSuccess,
    ExitDiffs,
    ExitNoDiffs
};

enum MergeAction {
    MergeActionNoPrompt = 0,
    MergeActionMerge    = 'm',
    MergeActionAdd      = 'a',
    MergeActionDelete   = 'd',
    MergeActionNothing  = 'n',
    MergeActionReplace  = 'r'
};

enum FileType {
    SymLink,
    Directory,
    RealFile
};

enum MergeParentState {
  MergeStateIncomplete       = 0x1,
  MergeStateParent           = 0x2,
  MergeStateIncompleteParent = 0x3
};

#if defined(__GNUG__) || defined(__MWERKS__)
#ifndef __REMOVE_ME_WHEN_THESE_COMPILERS_DO_FORWARD_TEMPLATE_DECLS__
/* Forward template declarations don't work in g++ 2.7.x or mwcc 2.01,
 * so we have unneccesary includes here. */
#include "dstring.h"
#include "dynarray.h"
#include "hash.h"
#include "populate.h"
#include "prcserror.h"
#endif
#endif

class PrVoidError;                    /* include/prcserror.h */
template <class Type> class PrError;  /* include/prcserror.h */

#if defined(PRCS_DEVEL) && defined(__GNUG__)
class NprVoidError;                   /* include/prcserror.h */
template <class Type> class NprError; /* include/prcserror.h */
#else
typedef PrVoidError NprVoidError;
#endif

class Dstring;            /* include/dstring.h */
class Estring;            /* include/projdesc.h */
class UpgradeRepository;  /* include/convert.h */
class ProjectDescriptor;  /* include/projdesc.h */
class RepEntry;           /* include/repository.h */
class FileEntry;          /* include/fileent.h */
class AdvisoryLock;       /* include/lock.h */
class PrettyOstream;      /* include/prcserror.h */
class PrettyStreambuf;    /* include/prcserror.h */
class Sexp;               /* include/sexp.h */
class PipeRec;            /* include/syscmd.h */
class DelayedJob;         /* include/syscmd.h */
class SystemCommand;      /* include/syscmd.h */
class RcsDelta;           /* include/vc.h */
class RcsVersionData;     /* include/vc.h */
class ProjectVersionData; /* include/vc.h */
class FileRecord;         /* include/populate.h */
class Dir;                /* include/prcsdir.h */
class MemorySegment;      /* include/memseg.h */
class RebuildFile;        /* include/rebuild.h */
class RepFreeFilename;    /* include/repository.h */
class QuickElim;          /* include/quick.h */
class MergeParentFile;
class MergeParentEntry;
class PrjFields;
class ListMarker;
class QuickElimEntry;
class MergeCandidate;
class PrcsAttrs;

template <class T, int DefaultSize, bool ZeroTerm> class Dynarray; /* include/dynarray.h */
template <class Key, class Data> class HashTable; /* include/hash.h */
template <class Key, class Data> class OrderedTable; /* include/hash.h */
template <class X, class Y> class Pair; /* include/hash.h */
template <class T> class List;          /* include/hash.h */

typedef Dynarray< char const*, 64, true> ArgList;
typedef Dynarray< char const*, 64, true> CharPtrArray;
typedef Dynarray< RcsVersionData*, 64, false > RcsVersionDataPtrArray;
typedef Dynarray< RcsVersionDataPtrArray*, 64, false > RcsVersionDataPtrArrayPtrArray;
typedef Dynarray< ProjectVersionData*, 64, false > ProjectVersionDataPtrArray;
typedef Dynarray< RcsDelta*, 64, false > RcsDeltaPtrArray;
typedef Dynarray< Dstring*, 64, false > DstringPtrArray;
typedef Dynarray< FileEntry*, 64, false > FileEntryPtrArray;
typedef Dynarray< MergeParentEntry*, 4, false > MergeParentEntryPtrArray;
typedef Dynarray< MergeParentFile*, 64, false > MergeParentFilePtrArray;
typedef Dynarray< const ListMarker*, 4, false > ConstListMarkerPtrArray;
typedef Dynarray< MergeCandidate*, 4, false > MergeCandidatePtrArray;
typedef Dynarray< int, 4, false > IntArray;

typedef HashTable< const char* , FileType > PathTable;
typedef HashTable< ino_t , FileType > InoTable;
typedef HashTable< const char*, Sexp* > SexpTable;
typedef HashTable< const char* , FileEntry* > FileTable;
typedef HashTable< const char*, RcsDelta* > RcsFileTable;
typedef HashTable< const char*, MergeParentFile* > MergeParentTable;
typedef HashTable< const char*, QuickElimEntry* > QuickElimTable;
typedef HashTable< const char*, const char* > KeywordTable;
typedef HashTable< FileEntry*, FileEntry* > EntryTable;
typedef HashTable< const PrcsAttrs*, PrcsAttrs* > AttrsTable;
typedef HashTable< const char*, SystemCommand* > CommandTable;

typedef OrderedTable< const char*, const char* > OrderedStringTable;

typedef List< FileRecord > FileRecordList;
typedef List< MemorySegment* > MemorySegmentList;
typedef List< DelayedJob* > DelayedJobList;
typedef List< void* > VoidPtrList;
typedef List< const ListMarker* > ListMarkerList;

typedef PrError< ArgList* > PrArgListPtrError;
typedef PrError< int > PrExitStatusError;
typedef PrError< int > PrIntError;
typedef PrError< pid_t > PrPidTError;
typedef PrError< PrcsExitStatus > PrPrcsExitStatusError;
typedef PrError< time_t > PrTimeTError;
typedef PrError< bool > PrBoolError;
typedef PrError< const char* > PrConstCharPtrError;
typedef PrError< Dstring* > PrDstringPtrError;
typedef PrError< ProjectDescriptor* > PrProjectDescriptorPtrError;
typedef PrError< RepEntry* > PrRepEntryPtrError;
typedef PrError< PipeRec* > PrPipeRecPtrError;
typedef PrError< FILE* > PrCFilePtrError;
typedef PrError< const Sexp* > PrConstSexpPtrError;
typedef PrError< RcsVersionData* > PrRcsVersionDataPtrError;
typedef PrError< ProjectVersionData* > PrProjectVersionDataPtrError;
typedef PrError< RcsDelta* > PrRcsDeltaPtrError;
typedef PrError< RcsVersionDataPtrArray* > PrRcsVersionDataPtrArrayPtrError;
typedef PrError< ProjectVersionDataPtrArray* > PrProjectVersionDataPtrArrayPtrError;
typedef PrError< RcsDeltaPtrArray* > PrRcsDeltaPtrArrayPtrError;
typedef PrError< RcsFileTable* > PrRcsFileTablePtrError;
typedef PrError< QuickElim* > PrQuickElimPtrError;
typedef PrError< RebuildFile* > PrRebuildFilePtrError;
typedef PrError< OverwriteStatus > PrOverwriteStatusError;
typedef PrError< FileEntry* > PrFileEntryPtrError;
typedef PrError< const PrcsAttrs* > PrPrcsAttrsPtrError;

#if defined(PRCS_DEVEL) && defined(__GNUG__)
typedef NprError< ArgList* > NprArgListPtrError;
typedef NprError< int > NprExitStatusError;
typedef NprError< int > NprIntError;
typedef NprError< PrcsExitStatus > NprPrcsExitStatusError;
typedef NprError< time_t > NprTimeTError;
typedef NprError< bool > NprBoolError;
typedef NprError< const char* > NprConstCharPtrError;
typedef NprError< Dstring* > NprDstringPtrError;
typedef NprError< ProjectDescriptor* > NprProjectDescriptorPtrError;
typedef NprError< RepEntry* > NprRepEntryPtrError;
typedef NprError< PipeRec* > NprPipeRecPtrError;
typedef NprError< FILE* > NprCFilePtrError;
typedef NprError< RcsVersionData* > NprRcsVersionDataPtrError;
typedef NprError< ProjectVersionData* > NprProjectVersionDataPtrError;
typedef NprError< RcsDelta* > NprRcsDeltaPtrError;
typedef NprError< RcsVersionDataPtrArray* > NprRcsVersionDataPtrArrayPtrError;
typedef NprError< ProjectVersionDataPtrArray* > NprProjectVersionDataPtrArrayPtrError;
typedef NprError< RcsDeltaPtrArray* > NprRcsDeltaPtrArrayPtrError;
typedef NprError< RcsFileTable* > NprRcsFileTablePtrError;
typedef NprError< QuickElim* > NprQuickElimPtrError;
typedef NprError< RebuildFile* > NprRebuildFilePtrError;
typedef NprError< OverwriteStatus > NprOverwriteStatusError;
typedef NprError< FILE* > NprCFilePtrError;
typedef NprError< char > NprCharError;
#else
typedef PrArgListPtrError NprArgListPtrError;
typedef PrExitStatusError NprExitStatusError;
typedef PrIntError NprIntError;
typedef PrPrcsExitStatusError NprPrcsExitStatusError;
typedef PrTimeTError NprTimeTError;
typedef PrBoolError NprBoolError;
typedef PrConstCharPtrError NprConstCharPtrError;
typedef PrDstringPtrError NprDstringPtrError;
typedef PrProjectDescriptorPtrError NprProjectDescriptorPtrError;
typedef PrRepEntryPtrError NprRepEntryPtrError;
typedef PrPipeRecPtrError NprPipeRecPtrError;
typedef PrCFilePtrError NprCFilePtrError;
typedef PrRcsVersionDataPtrError NprRcsVersionDataPtrError;
typedef PrProjectVersionDataPtrError NprProjectVersionDataPtrError;
typedef PrRcsDeltaPtrError NprRcsDeltaPtrError;
typedef PrRcsVersionDataPtrArrayPtrError NprRcsVersionDataPtrArrayPtrError;
typedef PrProjectVersionDataPtrArrayPtrError NprProjectVersionDataPtrArrayPtrError;
typedef PrRcsDeltaPtrArrayPtrError NprRcsDeltaPtrArrayPtrError;
typedef PrRcsFileTablePtrError NprRcsFileTablePtrError;
typedef PrQuickElimPtrError NprQuickElimPtrError;
typedef PrRebuildFilePtrError NprRebuildFilePtrError;
typedef PrOverwriteStatusError NprOverwriteStatusError;
typedef PrCFilePtrError NprCFilePtrError;
#endif

#endif

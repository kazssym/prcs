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

#if defined(PRCS_DEVEL) && defined(__GNUG__)
template class NprError<int>;
template class NprError<FILE*>;
template class NprError<const char*>;
template class NprError<QuickElim*>;
template class NprError<bool>;
#else
template class PrError<QuickElim*>;
#endif
template class PrError<int>;
template class PrError<short>;
template class PrError<long>;
template class PrError<PrcsExitStatus>;
template class PrError<ArgList*>;
template class PrError<const char*>;
template class PrError<PipeRec*>;
template class PrError<bool>;
template class PrError<FILE*>;
template class PrError<RepEntry*>;
template class PrError<ProjectVersionDataPtrArray*>;
template class PrError<RcsDelta*>;
template class PrError<ProjectVersionData*>;
template class PrError<RcsVersionData*>;
template class PrError<RcsFileTable*>;
template class PrError<char>;
template class PrError<const Sexp*>;
template class PrError<ProjectDescriptor*>;
template class PrError<RebuildFile*>;
template class PrError<OverwriteStatus>;
template class PrError<Dstring*>;
template class PrError<FileEntry*>;
template class PrError<const PrcsAttrs*>;

#if defined(__GNUG__) && __GNUC__ >= 2 && __GNUC_MINOR__ < 8
#define WACK
#else
#define WACK <>
#endif

#define MkTemplate(errtype, tclass) \
    template const errtype<tclass>& \
    operator<< WACK(tclass&, const errtype<tclass>&)

#if defined(PRCS_DEVEL) && defined(__GNUG__)
MkTemplate(NprError, FILE*);
MkTemplate(NprError, const char*);
MkTemplate(NprError, int);
MkTemplate(NprError, QuickElim*);
MkTemplate(NprError, bool);
#else
MkTemplate(PrError, QuickElim*);
#endif
MkTemplate(PrError, ArgList*);
MkTemplate(PrError, const char*);
MkTemplate(PrError, FILE*);
MkTemplate(PrError, short);
MkTemplate(PrError, int);
MkTemplate(PrError, long);
MkTemplate(PrError, bool);
MkTemplate(PrError, RepEntry*);
MkTemplate(PrError, RcsDelta*);
MkTemplate(PrError, ProjectVersionDataPtrArray*);
MkTemplate(PrError, ProjectVersionData*);
MkTemplate(PrError, RcsVersionData*);
MkTemplate(PrError, RcsFileTable*);
MkTemplate(PrError, char);
MkTemplate(PrError, const Sexp*);
MkTemplate(PrError, ProjectDescriptor*);
MkTemplate(PrError, RebuildFile*);
MkTemplate(PrError, OverwriteStatus);
MkTemplate(PrError, Dstring*);
MkTemplate(PrError, FileEntry*);
MkTemplate(PrError, const PrcsAttrs*);

#undef MkTemplate
#define MkTemplate(tclass) \
    template class omanip<tclass>; \
    template ostream& operator<< WACK(ostream&, const omanip<tclass>&)

MkTemplate(const char*);
MkTemplate(CharAndStr);
MkTemplate(int);
MkTemplate(QueryOption);
MkTemplate(FileLine);
MkTemplate(BangFlag*);
MkTemplate(FileTuple);

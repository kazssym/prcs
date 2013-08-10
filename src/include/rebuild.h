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
 * $Id: rebuild.h 1.9.1.3 Fri, 04 Apr 1997 02:12:20 -0800 jmacd $
 */


#ifndef _REBUILD_H_
#define _REBUILD_H_

extern const char prcs_data_file_name[];

PrVoidError admin_rebuild_command_no_open(RepEntry* rep, bool valid_rep_data);

PrRebuildFilePtrError read_repository_data(const char* filename0, bool write);

PrVoidError create_empty_data(const char* project, const char* filename0);

class RebuildFile {
public:

    friend PrRebuildFilePtrError read_repository_data(const char* filename0,
						      bool write);

    ~RebuildFile();

    ProjectVersionDataPtrArray* get_project_summary() const;
    RcsFileTable* get_rcs_file_summary() const;

    void add_project_data(ProjectVersionData* data);
    void add_rcs_file_data(const char* filename, RcsVersionData* data);

    PrVoidError update_project_data();

private:

    PrVoidError init_from_file(const char*, bool);

    PrVoidError read_header();
    PrVoidError read_rcs_file_summary();
    PrVoidError read_project_summary(int& version_count);

    PrRcsVersionDataPtrError read_rcs_version();
    PrProjectVersionDataPtrError read_project_version(int i);

    PrVoidError bad_data_file();

    RebuildFile();

    const char* get_string(); /* null if fail */
    const char* get_string(int len);
    /* expect a string, return false if failure */
    bool get_string(const char*, bool term = false);
    int get_size(); /* < 0 if fail */
    const char* source_name(); /* returns filename */
    bool done(); /* true if file has been completely read */
    void init_stream(); /* initialize os for writing */

    int offset;
    Dstring filename;
    const char* last;
    MemorySegment *seg;

    ProjectVersionDataPtrArray* project_data;
    RcsFileTable* rcs_file_table;

    streambuf* buf;
    ostream* os;
};

char* md5_buffer(const char* buffer, int buflen);

#endif

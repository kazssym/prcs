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

#include "prcs.h"

#include "convert.h"
#include "repository.h"
#include "misc.h"
#include "rebuild.h"


static const char description0_5to0_13[] =
"The repository data file's content is reduced somewhat from the "
"0.13.5 format";

static const char description1_0_4to1_0_5[] =
"A bug causing some of the bits in certain MD5 digests to be "
"thrown out has been fixed, the repository database must be rebuilt";

static const char description1_0_9to1_1_0[] =
"The repository data file's format changes slightly to allow "
"project renaming";

static const char description1_1_0to1_2_0[] =
"The repository data file's format changes slightly to accomodate "
"the new merge algorithm";

static PrBoolError just_rebuild(RepEntry* rep)
{
    Return_if_fail(admin_rebuild_command_no_open(rep, false));

    return true; /* should exit */
}

/*
 * repositoryUpgrades, entryUpgrades --
 *
 *     Each repository root and entry contains a file named prcs_tag
 *     which saves highest major, minor, and micro version numbers
 *     that last used it and the highest major, minor, and micro
 *     version numbers that may use this repository.
 *
 *     If a PRCS opens a repository or entry which was last used by a
 *     higher version number than itself but still higher than the
 *     usable by field, it will print a warning and any important
 *     upgrade information and continue.  If a PRCS opens a repository
 *     or entry which is not usable, it will print an error and exit.
 *
 *     However, each time a repository is opened which is tagged at a
 *     lower version number, it looks in these arrays for upgrade
 *     functions.  the upgrade functions are grouped with a set of
 *     version numbers that it will upgrade a repository to.  They
 *     must be in order.  If PRCS opens a repository or entry with an
 *     old tag, it will apply all applicable upgrades (with the
 *     users's consent).  */
UpgradeRepository entry_upgrades[] = {
    {{0,13,10},{1,1,0},just_rebuild,description0_5to0_13},
    {{1,0,5},  {1,1,0},just_rebuild,description1_0_4to1_0_5},
    {{1,1,0},  {1,1,0},just_rebuild,description1_0_9to1_1_0},
    {{1,2,0},  {1,2,0},just_rebuild,description1_1_0to1_2_0},
    {{0,0,0},  {0,0,0},NULL, NULL},
};

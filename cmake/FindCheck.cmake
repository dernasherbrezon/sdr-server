# - Try to find the CHECK libraries
#  Once done this will define
#
#  CHECK_FOUND - system has check
#  CHECK_INCLUDE_DIRS - the check include directory
#  CHECK_LIBRARIES - check library
#
#  This configuration file for finding libcheck is originally from
#  the opensync project. The originally was downloaded from here:
#  opensync.org/browser/branches/3rd-party-cmake-modules/modules/FindCheck.cmake
#
#  Copyright (c) 2007 Daniel Gollub <dgollub@suse.de>
#  Copyright (c) 2007 Bjoern Ricks  <b.ricks@fh-osnabrueck.de>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.

find_package(PkgConfig)
pkg_check_modules(PC_CHECK QUIET check)

find_path(CHECK_INCLUDE_DIR NAMES check.h)

find_library(CHECK_LIBRARY NAMES check
	HINTS ${PC_CHECK_LIBDIR} ${PC_CHECK_LIBRARY_DIRS})

set(CHECK_LIBRARIES ${CHECK_LIBRARY})
set(CHECK_INCLUDE_DIRS ${CHECK_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(Check DEFAULT_MSG CHECK_LIBRARY CHECK_INCLUDE_DIR)

mark_as_advanced(CHECK_INCLUDE_DIR CHECK_LIBRARY)

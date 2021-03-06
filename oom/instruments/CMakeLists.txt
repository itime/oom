#=============================================================================
#  OOMidi
#  OpenOctave Midi and Audio Editor
#  $Id:$
#
#  Copyright (C) 2002-2006 by Werner Schweer and others
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#=============================================================================

##
## Expand Qt macros in source files
##
QT4_WRAP_CPP ( instruments_mocs
   editinstrument.h
   # minstrument.h
   )

if(LSCP_SUPPORT)
QT4_WRAP_CPP ( instruments_mocs
	importinstruments.h
	${instruments_mocs}
      )
endif(LSCP_SUPPORT)


##
## UI files
##
file (GLOB instruments_ui_files
      # ccontrollerbase.ui # not built. It needs to be converted to Qt4 for revival.
      editinstrumentbase.ui
      )

if(LSCP_SUPPORT)
file (GLOB instruments_ui_files
	importbase.ui
	${instruments_ui_files}
)
endif(LSCP_SUPPORT)

QT4_WRAP_UI ( instruments_uis ${instruments_ui_files} )


##
## List of source files to compile
##
file (GLOB instruments_source_files
   editinstrument.cpp
   editinstrument.h
   minstrument.cpp
   minstrument.h
   )

if(LSCP_SUPPORT)
file (GLOB instruments_source_files
	importinstruments.cpp
	importinstruments.h
	${instruments_source_files}
)
endif(LSCP_SUPPORT)
##
## Define target
##
add_library ( instruments STATIC
   ${instruments_source_files}
   ${instruments_mocs}
   ${instruments_uis}
   )

##
## Append to the list of translations
##
set (FILES_TO_TRANSLATE
      ${FILES_TO_TRANSLATE}
      ${instruments_source_files}
      ${instruments_ui_files}
      CACHE INTERNAL ""
      )

##
## Compilation flags and target name
##
set_target_properties( instruments
      PROPERTIES COMPILE_FLAGS "-include ${PROJECT_BINARY_DIR}/all.h -fPIC"
      OUTPUT_NAME oom_instruments
      )

##
## Linkage
##
target_link_libraries ( instruments
      ${QT_LIBRARIES}
      icons
      )

if(LSCP_SUPPORT)
      target_link_libraries(instruments ${LSCP_LIBRARIES})
endif(LSCP_SUPPORT)

##
## Install location
##
#install(TARGETS instruments
#     DESTINATION ${OOMidi_MODULES_DIR}
#     )

